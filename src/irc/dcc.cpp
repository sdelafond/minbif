/*
 * Copyright(C) 2009 Romain Bignon
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>

#include "dcc.h"
#include "nick.h"
#include "message.h"
#include "core/callback.h"
#include "core/util.h"
#include "core/log.h"
#include "core/config.h"

namespace irc {

DCCServer::DCCServer(string _type, string _filename, size_t _total_size, Nick* _sender, Nick* _receiver)
	: type(_type),
	  filename(_filename),
	  total_size(_total_size),
	  start_time(time(NULL)),
	  sender(_sender),
	  receiver(_receiver),
	  listen_data(NULL),
	  watcher(0),
	  fd(-1),
	  port(0),
	  finished(false)
{
	ConfigItem* item = conf.GetSection("file_transfers")->GetItem("port_range");
	listen_data = purple_network_listen_range((uint16_t)item->MinInteger(), (uint16_t)item->MaxInteger(),
	                                          SOCK_STREAM, &DCCServer::listen_cb, this);
	if(!listen_data)
		throw DCCListenError();
}

DCCServer::~DCCServer()
{
	deinit();
}

void DCCServer::deinit()
{
	if(fd >= 0)
		close(fd);
	if(watcher > 0)
		purple_input_remove(watcher);
	if(listen_data != NULL)
		purple_network_listen_cancel(listen_data);

	finished = true;
	fd = -1;
	watcher = 0;
	listen_data = NULL;
}

void DCCServer::dcc_read_cb(gpointer data, int source, PurpleInputCondition cond)
{
	DCCServer* dcc = static_cast<DCCServer*>(data);
	dcc->dcc_read(source);
}

void DCCServer::connected(gpointer data, int source, PurpleInputCondition cond)
{
	DCCServer* dcc = static_cast<DCCServer*>(data);
	int conn = accept(dcc->fd, NULL, 0), flags;
	if(!conn)
	{
		b_log[W_ERR] << "DCC connection failed";
		dcc->deinit();
		return;
	}

	purple_input_remove(dcc->watcher);
	dcc->watcher = 0;
	close(dcc->fd);
	dcc->fd = conn;

	flags = fcntl(conn, F_GETFL);
	fcntl(conn, F_SETFL, flags | O_NONBLOCK);
	fcntl(conn, F_SETFD, FD_CLOEXEC);
	dcc->watcher = purple_input_add(conn, PURPLE_INPUT_READ, DCCServer::dcc_read_cb, dcc);

	dcc->updated(false);
}

void DCCServer::listen_cb(int sock, void* data)
{
	DCCServer* dcc = static_cast<DCCServer*>(data);
	struct in_addr addr;
	string bind_addr = conf.GetSection("file_transfers")->GetItem("dcc_own_ip")->String();
	if (bind_addr == " ")
		bind_addr = purple_network_get_my_ip(-1);

	dcc->fd = sock;
	dcc->listen_data = NULL;
	dcc->port = purple_network_get_port_from_fd(sock);
	if (!inet_aton(bind_addr.c_str(), &addr))
	{
		b_log[W_ERR] << "Unable to parse this IP address: [" << bind_addr << "]: Unable to send DCC request.";
		dcc->deinit();
		return;
	}

	dcc->watcher = purple_input_add(sock, PURPLE_INPUT_READ,
					connected, dcc);

	/* As there isn't any way to escape correctly strings in the DCC SEND
	 * sequence, it replaces every '"' with a '\''.
	 */
	string filename = dcc->filename;
	for(string::iterator c = filename.begin(); c != filename.end(); ++c)
		if(*c == '"') *c = '\'';

	dcc->receiver->send(Message(MSG_PRIVMSG).setSender(dcc->sender ? dcc->sender->getLongName() : "some.one")
			                 .setReceiver(dcc->receiver)
					 .addArg("\001DCC " + dcc->type + " \"" + dcc->filename + "\" " +
						 t2s(ntohl(addr.s_addr)) + " " +
						 t2s(dcc->port) +
						 (dcc->total_size ? (" " + t2s(dcc->total_size)) : "") +
						 "\001"));
}

DCCSend::DCCSend(const im::FileTransfert& _ft, Nick* _sender, Nick* _receiver)
	: DCCServer("SEND", _ft.getFileName(), _ft.getSize(), _sender, _receiver),
	  ft(_ft),
	  local_filename(_ft.getLocalFileName()),
	  bytes_sent(0),
	  fp(NULL),
	  rxlen(0),
	  rxqueue(NULL)
{
}

DCCSend::~DCCSend()
{
	deinit();
}

void DCCSend::deinit()
{
	DCCServer::deinit();

	if(fp != NULL)
		fclose(fp);
	g_free(rxqueue);
	fp = NULL;
	rxqueue = NULL;
}

void DCCSend::updated(bool destroy)
{
	if(destroy)
		this->ft = im::FileTransfert(); /* No-valid object */

	if((fd < 0 || listen_data) && (start_time + TIMEOUT < time(NULL)))
		deinit();
	else
		dcc_send();
}

void DCCSend::dcc_send()
{
	if(finished || listen_data || fd < 0)
		return;

	if(!fp)
	{
		fp = fopen(local_filename.c_str(), "r");
		if(!fp)
			return; /* File isn't written yet. */
	}

	/* It does not add any \0 on buffer */
	static char buf[512];
	size_t len = fread(buf, sizeof(char), sizeof buf, fp);

	if(len > 0)
		send(fd, buf, len, 0);

	bytes_sent += len;
}

void DCCSend::dcc_read(int source)
{
	static char buffer[64];
	ssize_t len;

	len = read(source, buffer, sizeof(buffer));

	if (len < 0 && errno == EAGAIN)
		return;
	else if (len <= 0) {
		/* DCC user has closed connection.
		 * fd is already closed, do not let deinit()
		 * reclose it.
		 */
		this->fd = -1;
		this->deinit();
		return;
	}

	this->rxqueue = (guchar*)g_realloc(this->rxqueue, len + this->rxlen);
	memcpy(this->rxqueue + this->rxlen, buffer, len);
	this->rxlen += (guint)len;

	while (1) {
		size_t acked;

		if (this->rxlen < 4)
			break;

		acked = ntohl(*((gint32 *)this->rxqueue));

		this->rxlen -= 4;
		if (this->rxlen) {
			unsigned char *tmp = (unsigned char*)g_memdup(this->rxqueue + 4, (guint)this->rxlen);
			g_free(this->rxqueue);
			this->rxqueue = tmp;
		} else {
			g_free(this->rxqueue);
			this->rxqueue = NULL;
		}

		if (acked >= this->total_size) {
			/* DCC send terminated \o/ */
			this->deinit();
			return;
		}
	}

	this->dcc_send();
}

DCCChat::DCCChat(Nick* sender, Nick* receiver)
	: DCCServer("CHAT", "CHAT", 0, sender, receiver)
{}

DCCChat::~DCCChat()
{

}

void DCCChat::deinit()
{
	DCCServer::deinit();
}

void DCCChat::dcc_read(int source)
{
	/* Don't care */
}

void DCCChat::updated(bool destroy)
{
	if((fd < 0 || listen_data) && (start_time + TIMEOUT < time(NULL)))
		deinit();
}

void DCCChat::dcc_send(string buf)
{
	if(finished || listen_data || fd < 0)
		return;

	send(fd, buf.c_str(), buf.size(), 0);
}

DCCGet::DCCGet(Nick* _from, string _filename, uint32_t addr, uint16_t port,
	       ssize_t size, _CallBack* _cb)
	: from(_from),
	  filename(_filename),
	  callback(_cb),
	  finished(false),
	  sock(-1),
	  watcher(0),
	  fp(NULL),
	  bytes_received(0),
	  total_size(size)
{
	fp = fopen(filename.c_str(), "w");
	if(!fp)
	{
		b_log[W_ERR] << "Unable to create local file: " << filename;
		throw DCCGetError();
	}

	struct sockaddr_in fsocket;

	memset(&fsocket, 0, sizeof fsocket);
	fsocket.sin_family = AF_INET;
	fsocket.sin_addr.s_addr = htonl(addr);
	fsocket.sin_port = htons(port);

	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if(connect(sock, (struct sockaddr*) &fsocket, sizeof fsocket) < 0)
	{
		b_log[W_ERR] << "Unable to receive file: " << strerror(errno);
		delete callback;
		throw DCCGetError();
	}

	watcher = purple_input_add(sock, PURPLE_INPUT_READ, DCCGet::dcc_read, this);
}

DCCGet::~DCCGet()
{
	deinit();
}

void DCCGet::deinit()
{
	if(sock >= 0)
		close(sock);
	if(watcher > 0)
		purple_input_remove(watcher);
	if(fp != NULL)
		fclose(fp);
	if(callback)
		delete callback;

	finished = true;
	sock = -1;
	watcher = 0;
	fp = NULL;
	callback = NULL;
}

void DCCGet::dcc_read(gpointer data, int source, PurpleInputCondition cond)
{
	DCCGet* dcc = static_cast<DCCGet*>(data);
	static char buffer[1024];
	ssize_t len;

	len = read(source, buffer, sizeof(buffer));

	if (len < 0 && errno == EAGAIN)
		return;
	else if (len <= 0) {
		/* DCC user has closed connection.
		 * fd is already closed, do not let deinit()
		 * reclose it.
		 */
		dcc->sock = -1;
		dcc->deinit();
		return;
	}

	if(fwrite(buffer, sizeof(char), len, dcc->fp) < (size_t)len)
	{
		b_log[W_ERR] << "Unable to write received data: " << strerror(errno);
		dcc->deinit();
		return;
	}

	dcc->bytes_received += len;
	if(dcc->bytes_received >= dcc->total_size)
	{
		unsigned long l = htonl(dcc->bytes_received);
		size_t result = write(dcc->sock, &l, sizeof(l));
		if(result != sizeof(l))
			b_log[W_WARNING] << "Unable to send DCC ack";

		if(dcc->callback)
		{
			fflush(dcc->fp);
			dcc->callback->run();
		}
		dcc->deinit();
	}

}

void DCCGet::updated(bool destroy)
{
	/* I don't care about. */
}

void DCCGet::setPeer(Nick* n)
{
	/* IRC user left, can remove transfert. */
	from = n;
	if(from) /* I think this never happens... */
		callback->setObj(from);
	else
		deinit();
}

bool DCCGet::parseDCCSEND(string line, string* filename, uint32_t* addr, uint16_t* port, ssize_t* size)
{
	if(line[0] != '\1' || line[line.size() - 1] != '\1')
		return false;

	string word;
	Message args;

	/* Remove \1 chars. */
	line = line.substr(1, line.size()-2);

	while((word = stringtok(line, " ")).empty() == false)
		args.addArg(word);
	args.rebuildWithQuotes();

	if(args.countArgs() == 6 && args.getArg(0) == "DCC" && args.getArg(1) == "SEND")
	{
		*filename = args.getArg(2);
		*addr = s2t<uint32_t>(args.getArg(3));
		*port = s2t<uint16_t>(args.getArg(4));
		*size = s2t<ssize_t>(args.getArg(5));
		return true;
	}

	return false;
}

} /* namespace irc */
