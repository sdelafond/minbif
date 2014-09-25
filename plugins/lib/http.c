/*
 * libfacebook is the property of its developers.  See the COPYRIGHT file
 * for more details.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "http.h"
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

static void http_attempt_connection(HttpConnection *);

HttpHandler* http_handler_new(PurpleAccount* account, void* data)
{
	HttpHandler* handler = g_new0(HttpHandler, 1);
	handler->account = account;
	handler->pc = purple_account_get_connection(account);
	handler->cookie_table = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, g_free);
	handler->hostname_ip_cache = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, g_free);
	handler->data = data;
	return handler;
}

void http_handler_free(HttpHandler* handler)
{
	purple_debug_info("httpproxy", "destroying %d incomplete connections\n",
			g_slist_length(handler->conns));

	while (handler->conns != NULL)
		http_connection_destroy(handler->conns->data);

	while (handler->dns_queries != NULL) {
		PurpleDnsQueryData *dns_query = handler->dns_queries->data;
		purple_debug_info("httpproxy", "canceling dns query for %s\n",
					purple_dnsquery_get_host(dns_query));
		handler->dns_queries = g_slist_remove(handler->dns_queries, dns_query);
		purple_dnsquery_destroy(dns_query);
	}

	g_hash_table_destroy(handler->cookie_table);
	g_hash_table_destroy(handler->hostname_ip_cache);
	g_free(handler);
}

#ifdef HAVE_ZLIB
static guchar *http_gunzip(const guchar *gzip_data, ssize_t *len_ptr)
{
	gsize gzip_data_len	= *len_ptr;
	z_stream zstr;
	int gzip_err = 0;
	guchar *output_data;
	gulong gzip_len = G_MAXUINT16;

	g_return_val_if_fail(zlib_inflate != NULL, NULL);

	output_data = g_new0(guchar, gzip_len);

	zstr.next_in = gzip_data;
	zstr.avail_in = gzip_data_len;
	zstr.zalloc = Z_NULL;
	zstr.zfree = Z_NULL;
	zstr.opaque = Z_NULL;
	int flags = gzip_data[3];
	int offset = 4;
	/* if (flags & 0x04) offset += *tmp[] */
	zstr.next_in += offset;
	zstr.avail_in -= offset;
	zlib_inflateInit2_(&zstr, -MAX_WBITS, ZLIB_VERSION, sizeof(z_stream));
	zstr.next_out = output_data;
	zstr.avail_out = gzip_len;
	gzip_err = zlib_inflate(&zstr, Z_FINISH);
	zlib_inflateEnd(&zstr);

	purple_debug_info("httpproxy", "gzip len: %ld, len: %ld\n", gzip_len,
			gzip_data_len);
	purple_debug_info("httpproxy", "gzip flags: %d\n", flags);
	purple_debug_info("httpproxy", "gzip error: %d\n", gzip_err);

	*len_ptr = gzip_len;
	return output_data;
}
#endif

void http_connection_destroy(HttpConnection *fbconn)
{
	fbconn->handler->conns = g_slist_remove(fbconn->handler->conns, fbconn);

	if (fbconn->request != NULL)
		g_string_free(fbconn->request, TRUE);

	g_free(fbconn->rx_buf);

	if (fbconn->connect_data != NULL)
		purple_proxy_connect_cancel(fbconn->connect_data);

	if (fbconn->ssl_conn != NULL)
		purple_ssl_close(fbconn->ssl_conn);

	if (fbconn->fd >= 0) {
		close(fbconn->fd);
	}

	if (fbconn->input_watcher > 0)
		purple_input_remove(fbconn->input_watcher);

	g_free(fbconn->hostname);
	g_free(fbconn);
}

static void http_update_cookies(HttpHandler *handler, const gchar *headers)
{
	const gchar *cookie_start;
	const gchar *cookie_end;
	gchar *cookie_name;
	gchar *cookie_value;
	int header_len;

	g_return_if_fail(headers != NULL);

	header_len = strlen(headers);

	/* look for the next "Set-Cookie: " */
	/* grab the data up until ';' */
	cookie_start = headers;
	while ((cookie_start = strstr(cookie_start, "\r\nSet-Cookie: ")) &&
			(headers-cookie_start) < header_len)
	{
		cookie_start += 14;
		cookie_end = strchr(cookie_start, '=');
		cookie_name = g_strndup(cookie_start, cookie_end-cookie_start);
		cookie_start = cookie_end + 1;
		cookie_end = strchr(cookie_start, ';');
		cookie_value= g_strndup(cookie_start, cookie_end-cookie_start);
		cookie_start = cookie_end;

		purple_debug_info("httpproxy", "got cookie %s=%s\n",
				cookie_name, cookie_value);

		g_hash_table_replace(handler->cookie_table, cookie_name,
				cookie_value);
	}
}

static void http_connection_process_data(HttpConnection *fbconn)
{
	ssize_t len;
	gchar *tmp;

	len = fbconn->rx_len;
	tmp = g_strstr_len(fbconn->rx_buf, len, "\r\n\r\n");
	if (tmp == NULL) {
		/* This is a corner case that occurs when the connection is
		 * prematurely closed either on the client or the server.
		 * This can either be no data at all or a partial set of
		 * headers.  We pass along the data to be good, but don't
		 * do any fancy massaging.  In all likelihood the result will
		 * be tossed by the connection callback func anyways
		 */
		tmp = g_strndup(fbconn->rx_buf, len);
	} else {
		tmp += 4;
		len -= g_strstr_len(fbconn->rx_buf, len, "\r\n\r\n") -
				fbconn->rx_buf + 4;
		tmp = g_memdup(tmp, len + 1);
		tmp[len] = '\0';
		fbconn->rx_buf[fbconn->rx_len - len] = '\0';
		purple_debug_misc("httpproxy", "response headers\n%s\n",
				fbconn->rx_buf);
		http_update_cookies(fbconn->handler, fbconn->rx_buf);

#ifdef HAVE_ZLIB
		if (strstr(fbconn->rx_buf, "Content-Encoding: gzip"))
		{
			/* we've received compressed gzip data, decompress */
			if (zlib_inflate != NULL)
			{
				gchar *gunzipped;
				gunzipped = http_gunzip((const guchar *)tmp, &len);
				g_free(tmp);
				tmp = gunzipped;
			}
		}
#endif
	}

	g_free(fbconn->rx_buf);
	fbconn->rx_buf = NULL;

	if (fbconn->callback != NULL)
		fbconn->callback(fbconn->handler, tmp, len, fbconn->user_data);

	g_free(tmp);
}

static void http_fatal_connection_cb(HttpConnection *fbconn)
{
	PurpleConnection *pc = fbconn->handler->pc;

	purple_debug_error("httpproxy", "fatal connection error\n");

	http_connection_destroy(fbconn);

	/* We died.  Do not pass Go.  Do not collect $200 */
	/* In all seriousness, don't attempt to call the normal callback here.
	 * That may lead to the wrong error message being displayed */
	purple_connection_error_reason(pc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				"Server closed the connection.");

}

static void http_post_or_get_readdata_cb(gpointer data, gint source,
		PurpleInputCondition cond)
{
	HttpConnection *fbconn;
	gchar buf[4096];
	ssize_t len;

	fbconn = data;

	if (fbconn->method & HTTP_METHOD_SSL) {
		len = purple_ssl_read(fbconn->ssl_conn,
				buf, sizeof(buf) - 1);
	} else {
		len = recv(fbconn->fd, buf, sizeof(buf) - 1, 0);
	}

	if (len < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
			/* Try again later */
			return;
		}

		if (fbconn->method & HTTP_METHOD_SSL && fbconn->rx_len > 0) {
			/*
			 * This is a slightly hacky workaround for a bug in either
			 * GNU TLS or in the SSL implementation on Http's web
			 * servers.  The sequence of events is:
			 * 1. We attempt to read the first time and successfully read
			 *    the server's response.
			 * 2. We attempt to read a second time and libpurple's call
			 *    to gnutls_record_recv() returns the error
			 *    GNUTLS_E_UNEXPECTED_PACKET_LENGTH, or
			 *    "A TLS packet with unexpected length was received."
			 *
			 * Normally the server would have closed the connection
			 * cleanly and this second read() request would have returned
			 * 0.  Or maybe it's normal for SSL connections to be severed
			 * in this manner?  In any case, this differs from the behavior
			 * of the standard recv() system call.
			 */
			purple_debug_warning("httpproxy",
				"ssl error, but data received.  attempting to continue\n");
		} else {
			/* TODO: Is this a regular occurrence?  If so then maybe resend the request? */
			http_fatal_connection_cb(fbconn);
			return;
		}
	}

	if (len > 0)
	{
		buf[len] = '\0';

		fbconn->rx_buf = g_realloc(fbconn->rx_buf,
				fbconn->rx_len + len + 1);
		memcpy(fbconn->rx_buf + fbconn->rx_len, buf, len + 1);
		fbconn->rx_len += len;

		/* Wait for more data before processing */
		return;
	}

	/* The server closed the connection, let's parse the data */
	http_connection_process_data(fbconn);

	http_connection_destroy(fbconn);
}

static void http_post_or_get_ssl_readdata_cb (gpointer data,
		PurpleSslConnection *ssl, PurpleInputCondition cond)
{
	http_post_or_get_readdata_cb(data, -1, cond);
}

static void http_post_or_get_connect_cb(gpointer data, gint source,
		const gchar *error_message)
{
	HttpConnection *fbconn;
	ssize_t len;

	fbconn = data;
	fbconn->connect_data = NULL;

	if (error_message)
	{
		purple_debug_error("httpproxy", "post_or_get_connect_cb %s\n", error_message);
		http_fatal_connection_cb(fbconn);
		return;
	}

	purple_debug_info("httpproxy", "post_or_get_connect_cb\n");
	fbconn->fd = source;

	len = write(fbconn->fd, fbconn->request->str, fbconn->request->len);
	if (len < 0 && errno == EAGAIN)
		return;
	else if (len <= 0) {
		purple_debug_error("httpproxy", "post_or_get_connect_cb %s\n", g_strerror(errno));
		http_fatal_connection_cb(fbconn);
		return;
	}

	fbconn->input_watcher = purple_input_add(fbconn->fd,
			PURPLE_INPUT_READ,
			http_post_or_get_readdata_cb, fbconn);
}

static void http_post_or_get_ssl_connect_cb(gpointer data,
		PurpleSslConnection *ssl, PurpleInputCondition cond)
{
	HttpConnection *fbconn;
	ssize_t len;

	fbconn = data;

	purple_debug_info("httpproxy", "post_or_get_ssl_connect_cb\n");

	len = purple_ssl_write(fbconn->ssl_conn, fbconn->request->str, fbconn->request->len);

	if (len < 0 && errno == EAGAIN)
		return;
	else if (len <= 0) {
		purple_debug_error("httpproxy", "post_or_get_ssl_connect_cb %s\n", g_strerror(errno));
		http_fatal_connection_cb(fbconn);
		return;
	}

	purple_ssl_input_add(fbconn->ssl_conn,
			http_post_or_get_ssl_readdata_cb, fbconn);
}

static void http_host_lookup_cb(GSList *hosts, gpointer data,
		const char *error_message)
{
	GSList *host_lookup_list;
	struct sockaddr_in *addr;
	gchar *hostname;
	gchar *ip_address = NULL;
	HttpHandler *handler;
	PurpleDnsQueryData *query;

	purple_debug_info("httpproxy", "updating cache of dns addresses\n");

	/* Extract variables */
	host_lookup_list = data;

	handler = host_lookup_list->data;
	host_lookup_list =
			g_slist_delete_link(host_lookup_list, host_lookup_list);
	hostname = host_lookup_list->data;
	host_lookup_list =
			g_slist_delete_link(host_lookup_list, host_lookup_list);
	query = host_lookup_list->data;
	host_lookup_list =
			g_slist_delete_link(host_lookup_list, host_lookup_list);

	/* The callback has executed, so we no longer need to keep track of
	 * the original query.  This always needs to run when the cb is
	 * executed. */
	handler->dns_queries = g_slist_remove(handler->dns_queries, query);

	/* Any problems, capt'n? */
	if (error_message != NULL)
	{
		purple_debug_warning("httpproxy",
				"Error doing host lookup: %s\n", error_message);
		return;
	}

	if (hosts == NULL)
	{
		purple_debug_warning("httpproxy",
				"Could not resolve host name\n");
		return;
	}

	/*
	 * DNS lookups can return a list of IP addresses, but we only cache
	 * the first one.  So free the rest.
	 */
	while (hosts != NULL)
	{
		/* Discard the length... */
		hosts = g_slist_delete_link(hosts, hosts);

		addr = hosts->data;
		if(!ip_address && addr->sin_addr.s_addr)
			ip_address = g_strdup(inet_ntoa(addr->sin_addr));

		/* Free the address... */
		g_free(hosts->data);
		hosts = g_slist_delete_link(hosts, hosts);
	}

	purple_debug_info("httpproxy", "Host %s has IP %s\n",
			hostname, ip_address);

	g_hash_table_insert(handler->hostname_ip_cache, hostname, ip_address);
}

static void http_cookie_foreach_cb(gchar *cookie_name,
		gchar *cookie_value, GString *str)
{
	/* TODO: Need to escape name and value? */
	g_string_append_printf(str, "%s=%s;", cookie_name, cookie_value);
}

/**
 * Serialize the handler->cookie_table hash table to a string.
 */
static gchar *http_cookies_to_string(HttpHandler *handler)
{
	GString *str;

	str = g_string_new(NULL);

	g_hash_table_foreach(handler->cookie_table,
			(GHFunc)http_cookie_foreach_cb, str);

	return g_string_free(str, FALSE);
}

static void http_ssl_connection_error(PurpleSslConnection *ssl,
		PurpleSslErrorType errortype, gpointer data)
{
	HttpConnection *fbconn = data;
	PurpleConnection *pc = fbconn->handler->pc;

	fbconn->ssl_conn = NULL;
	http_connection_destroy(fbconn);
	purple_connection_ssl_error(pc, errortype);
}

void http_post_or_get(HttpHandler *handler, HttpMethod method,
		const gchar *host, const gchar *url, const gchar *postdata,
		HttpProxyCallbackFunc callback_func, gpointer user_data,
		gboolean keepalive)
{
	GString *request;
	gchar *cookies;
	HttpConnection *fbconn;
	gchar *real_url;
	gboolean is_proxy = FALSE;
	const gchar *user_agent;
	const gchar* const *languages;
	gchar *language_names;

	/* TODO: Fix keepalive and use it as much as possible */
	keepalive = FALSE;

	if (host == NULL)
		host = "linuxfr.org";

	if (handler && handler->account && handler->account->proxy_info &&
		(handler->account->proxy_info->type == PURPLE_PROXY_HTTP ||
		(handler->account->proxy_info->type == PURPLE_PROXY_USE_GLOBAL &&
			purple_global_proxy_get_info() &&
			purple_global_proxy_get_info()->type ==
					PURPLE_PROXY_HTTP)))
	{
		real_url = g_strdup_printf("http://%s%s", host, url);
		is_proxy = TRUE;
	} else {
		real_url = g_strdup(url);
	}

	cookies = http_cookies_to_string(handler);
	user_agent = purple_account_get_string(handler->account, "user-agent", "libpurple");

	/* Build the request */
	request = g_string_new(NULL);
	g_string_append_printf(request, "%s %s HTTP/1.0\r\n",
			(method & HTTP_METHOD_POST) ? "POST" : "GET",
			real_url);
	g_string_append_printf(request, "Host: %s\r\n", host);
	g_string_append_printf(request, "Connection: %s\r\n",
			(keepalive ? "Keep-Alive" : "close"));
	g_string_append_printf(request, "User-Agent: %s\r\n", user_agent);
	if (method & HTTP_METHOD_POST) {
		g_string_append_printf(request,
				"Content-Type: application/x-www-form-urlencoded\r\n");
		g_string_append_printf(request,
				"Content-length: %zu\r\n", strlen(postdata));
	}
	g_string_append_printf(request, "Accept: */*\r\n");
	/* linuxfr accepts only connections from that referer. */
	g_string_append_printf(request, "Referer: http://%s%s/\r\n", host, real_url);
	g_string_append_printf(request, "Cookie: %s\r\n", cookies);
#ifdef HAVE_ZLIB
	if (zlib_inflate != NULL)
		g_string_append_printf(request, "Accept-Encoding: gzip\r\n");
#endif

	/* Tell the server what language we accept, so that we get error messages in our language (rather than our IP's) */
	languages = g_get_language_names();
	language_names = g_strjoinv(", ", (gchar **)languages);
	purple_util_chrreplace(language_names, '_', '-');
	g_string_append_printf(request, "Accept-Language: %s\r\n", language_names);
	g_free(language_names);

	purple_debug_misc("httpproxy", "sending request headers:\n%s\n",
			request->str);

	g_string_append_printf(request, "\r\n");
	if (method & HTTP_METHOD_POST)
		g_string_append_printf(request, "%s", postdata);

	/* If it needs to go over a SSL connection, we probably shouldn't print
	 * it in the debug log.  Without this condition a user's password is
	 * printed in the debug log */
	if (method == HTTP_METHOD_POST)
		purple_debug_misc("httpproxy", "sending request data:\n%s\n",
			postdata);

	g_free(cookies);
	g_free(real_url);
	/*
	 * Do a separate DNS lookup for the given host name and cache it
	 * for next time.
	 *
	 * TODO: It would be better if we did this before we call
	 *       purple_proxy_connect(), so we could re-use the result.
	 *       Or even better: Use persistent HTTP connections for servers
	 *       that we access continually.
	 *
	 * TODO: This cache of the hostname<-->IP address does not respect
	 *       the TTL returned by the DNS server.  We should expire things
	 *       from the cache after some amount of time.
	 */
	if (!is_proxy)
	{
		/* Don't do this for proxy connections, since proxies do the DNS lookup */
		gchar *host_ip;

		host_ip = g_hash_table_lookup(handler->hostname_ip_cache, host);
		if (host_ip != NULL) {
			purple_debug_info("httpproxy",
					"swapping original host %s with cached value of %s\n",
					host, host_ip);
			host = host_ip;
		} else if (handler->account && !handler->account->disconnecting) {
			GSList *host_lookup_list = NULL;
			PurpleDnsQueryData *query;

			host_lookup_list = g_slist_prepend(
					host_lookup_list, g_strdup(host));
			host_lookup_list = g_slist_prepend(
					host_lookup_list, handler);

			query = purple_dnsquery_a(host, 80,
					http_host_lookup_cb, host_lookup_list);
			handler->dns_queries = g_slist_prepend(handler->dns_queries, query);
			host_lookup_list = g_slist_append(host_lookup_list, query);
		}
	}

	fbconn = g_new0(HttpConnection, 1);
	fbconn->handler = handler;
	fbconn->method = method;
	fbconn->hostname = g_strdup(host);
	fbconn->request = request;
	fbconn->callback = callback_func;
	fbconn->user_data = user_data;
	fbconn->fd = -1;
	fbconn->connection_keepalive = keepalive;
	fbconn->request_time = time(NULL);
	handler->conns = g_slist_prepend(handler->conns, fbconn);

	http_attempt_connection(fbconn);
}

static void http_attempt_connection(HttpConnection *fbconn)
{
	HttpHandler *handler = fbconn->handler;

#if 0
	/* Connection to attempt retries.  This code doesn't work perfectly, but
	 * remains here for future reference if needed */
	if (time(NULL) - fbconn->request_time > 5) {
		/* We've continuously tried to remake this connection for a
		 * bit now.  It isn't happening, sadly.  Time to die. */
		purple_debug_error("httpproxy", "could not connect after retries\n");
		http_fatal_connection_cb(fbconn);
		return;
	}

	purple_debug_info("httpproxy", "making connection attempt\n");

	/* TODO: If we're retrying the connection, consider clearing the cached
	 * DNS value.  This will require some juggling with the hostname param */
	/* TODO/FIXME: This retries almost instantenously, which in some cases
	 * runs at blinding speed.  Slow it down. */
	/* TODO/FIXME: this doesn't retry properly on non-ssl connections */
#endif

	if (fbconn->method & HTTP_METHOD_SSL) {
		fbconn->ssl_conn = purple_ssl_connect(handler->account, fbconn->hostname,
				443, http_post_or_get_ssl_connect_cb,
				http_ssl_connection_error, fbconn);
	} else {
		fbconn->connect_data = purple_proxy_connect(NULL, handler->account,
				fbconn->hostname, 80, http_post_or_get_connect_cb, fbconn);
	}

	return;
}

char* http_url_encode(const char *string, int use_plus)
{
   int alloc=strlen(string)+1;
   char *ns = malloc(alloc);
   unsigned char in;
   int newlen = alloc;
   int index=0;

   while(*string) {
      in = *string;
      if(' ' == in && use_plus)
	 ns[index++] = '+';
      else if(!(in >= 'a' && in <= 'z') &&
	      !(in >= 'A' && in <= 'Z') &&
	      !(in >= '0' && in <= '9')) {
	 /* encode it */
	 newlen += 2; /* the size grows with two, since this'll become a %XX */
	 if(newlen > alloc) {
	    alloc *= 2;
	    ns = realloc(ns, alloc);
	    if(!ns)
	       return NULL;
	 }
	 sprintf(&ns[index], "%%%02X", in);
	 index+=3;
      }
      else {
	 /* just copy this */
	 ns[index++]=in;
      }
      string++;
   }
   ns[index]=0; /* terminate it */
   return ns;
}


