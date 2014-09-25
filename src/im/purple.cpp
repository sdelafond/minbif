/*
 * Minbif - IRC instant messaging gateway
 * Copyright(C) 2009-2010 Romain Bignon
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

#include <purple.h>
#include <cassert>

#include "purple.h"
#include "im.h"
#include "buddy.h"
#include "conversation.h"
#include "request.h"
#include "roomlist.h"
#include "ft.h"
#include "media.h"
#include "irc/irc.h"
#include "irc/buddy_icon.h"
#include "core/version.h"
#include "core/log.h"
#include "core/util.h"
#include "core/config.h"

namespace im {

IM* Purple::im = NULL;

PurpleEventLoopUiOps Purple::eventloop_ops =
{
	/* timeout_add */    g_timeout_add,
	/* timeout_remove */ g_source_remove,
	/* input_add */      glib_input_add,
	/* input_remove */   g_source_remove,
	/* input_get_error*/ NULL,
	/* timeout_add_seconds */ NULL,
	NULL, NULL, NULL
};

void Purple::debug(PurpleDebugLevel level, const char *category, const char *args)
{
	switch(level)
	{
		case PURPLE_DEBUG_FATAL:
			b_log[W_ERR] << "[" << category << "] " << args;
			break;
		case PURPLE_DEBUG_ERROR:
			b_log[W_PURPLE] << "[" << category << "] " << args;
			break;
		case PURPLE_DEBUG_WARNING:
			b_log[W_DEBUG] << "[" << category << "] " << args;
			break;
		default:
			break;
	}
}

PurpleDebugUiOps Purple::debug_ops =
{
        Purple::debug,
        NULL, //finch_debug_is_enabled,

        /* padding */
        NULL,
        NULL,
        NULL,
        NULL
};

void Purple::debug_init()
{
	purple_debug_set_ui_ops(&debug_ops);
}

void Purple::minbif_prefs_init()
{
	purple_prefs_add_none("/minbif");

	purple_prefs_add_string("/minbif/password", "");
	purple_prefs_add_int("/minbif/typing_notice", 0);
	purple_prefs_add_bool("/minbif/accept_nobuddies_messages", true);
	purple_prefs_add_int("/minbif/send_delay", 300);
	purple_prefs_add_bool("/minbif/voiced_buddies", true);
	purple_prefs_add_bool("/minbif/server_aliases", true);
}

GHashTable *Purple::ui_info = NULL;
GHashTable *Purple::minbif_ui_get_info(void)
{
	if (ui_info == NULL)
	{
		ui_info = g_hash_table_new(g_str_hash, g_str_equal);

		g_hash_table_insert(ui_info, (void*)"name",         (void*)MINBIF_VERSION_NAME);
		g_hash_table_insert(ui_info, (void*)"version",      (void*)MINBIF_VERSION);
		g_hash_table_insert(ui_info, (void*)"website",      (void*)MINBIF_WEBSITE);
		g_hash_table_insert(ui_info, (void*)"dev_website",  (void*)MINBIF_DEV_WEBSITE);
		g_hash_table_insert(ui_info, (void*)"client_type",  (void*)"pc");

	}

	return ui_info;
}


PurpleCoreUiOps Purple::core_ops =
{
	Purple::minbif_prefs_init,
        Purple::debug_init,
        Purple::inited,
        Purple::uninit,
        Purple::minbif_ui_get_info,

        /* padding */
        NULL,
        NULL,
        NULL
};

void Purple::init(IM* im)
{
	if(Purple::im)
		throw PurpleError("These is already a purple instance!");
	purple_util_set_user_dir(im->getUserPath().c_str());

	purple_core_set_ui_ops(&core_ops);
	purple_eventloop_set_ui_ops(&eventloop_ops);

	Purple::im = im;
	if (!purple_core_init(MINBIF_VERSION_NAME))
		throw PurpleError("Initialization of the Purple core failed.");

	/* XXX the currently implementation of this function works only with
	 * dbus, but minbif does not use it. */
	if (!purple_core_ensure_single_instance())
		throw PurpleError("You are already connected on this account with minbif.");

	purple_set_blist(purple_blist_new());
	purple_blist_load();
}

void Purple::inited()
{
	Account::init();
	RoomList::init();
	Buddy::init();
	Conversation::init();
	Request::init();
	FileTransfert::init();
	Media::init();

	irc::IRC* irc = getIM()->getIRC();
	irc::BuddyIcon* bi = new irc::BuddyIcon(getIM(), irc);
	irc->addNick(bi);

	bool conv_logs = conf.GetSection("logging")->GetItem("conv_logs")->Boolean();
	purple_prefs_set_bool("/purple/logging/log_ims", conv_logs);
	purple_prefs_set_bool("/purple/logging/log_chats", conv_logs);
	purple_prefs_set_bool("/purple/logging/log_system", conv_logs);
}

void Purple::uninit()
{
	assert(im != NULL);

	if(ui_info)
		g_hash_table_destroy(ui_info);
	Account::uninit();
	RoomList::uninit();
	Buddy::uninit();
	Conversation::uninit();
	Request::uninit();
	FileTransfert::uninit();
	Media::uninit();
}

map<string, Plugin> Purple::getPluginsList()
{
	map<string, Plugin> m;
	GList* list;

	purple_plugins_probe(G_MODULE_SUFFIX);

	for(list = purple_plugins_get_all(); list; list = list->next)
	{
		PurplePlugin* plugin = (PurplePlugin*)list->data;

		if (plugin->info->type == PURPLE_PLUGIN_LOADER)
		{
			GList *cur;
			for (cur = PURPLE_PLUGIN_LOADER_INFO(plugin)->exts; cur != NULL; cur = cur->next)
				purple_plugins_probe((const char*)cur->data);
			continue;
		}

		if (plugin->info->type != PURPLE_PLUGIN_STANDARD ||
		    plugin->info->flags & PURPLE_PLUGIN_FLAG_INVISIBLE)
			continue;

		m[plugin->info->id] = Plugin(plugin);
	}
	return m;
}

map<string, Protocol> Purple::getProtocolsList()
{
	map<string, Protocol> m;
	GList* list = purple_plugins_get_protocols();

	for(; list; list = list->next)
	{
		Protocol protocol = Protocol((PurplePlugin*)list->data);
		m[protocol.getID()] = protocol;
	}

	return m;
}

map<string, Account> Purple::getAccountsList()
{
	map<string, Account> m;
	GList* list = purple_accounts_get_all();
	map<string, Protocol> protocols = getProtocolsList();

	for(; list; list = list->next)
	{
		Protocol proto = getProtocolByPurpleID(((PurpleAccount*)list->data)->protocol_id);
		if(!proto.isValid())
			continue;

		Account account = Account((PurpleAccount*)list->data, proto);
		m[account.getID()] = account;
	}

	return m;
}

Protocol Purple::getProtocolByPurpleID(string id)
{
	GList* list = purple_plugins_get_protocols();

	for(; list; list = list->next)
		if(id == ((PurplePlugin*)list->data)->info->id)
			return Protocol((PurplePlugin*)list->data);

	return Protocol();
}

string Purple::getNewAccountName(Protocol proto, const Account& butone)
{
	GList* list = purple_accounts_get_all();
	GList* iter;
	int i = 0;

	for(iter = list; iter; iter = (iter ? iter->next : list))
	{
		Account acc((PurpleAccount*)iter->data);
		if(acc == butone || !acc.getProtocol().isValid() || acc.getProtocol() != proto)
			continue;

		string id = acc.getID(false);
		if(id == proto.getID() + (i ? t2s(i) : string()))
		{
			if (i)
				i = s2t<int>(id.substr(proto.getID().size())) + 1;
			else
				i = 1;
			iter = NULL; // restart
		}
	}
	return proto.getID() + (i ? t2s(i) : string());
}

Account Purple::addAccount(const Protocol& proto, const string& username, const Protocol::Options& options, bool register_account)
{
	PurpleAccount *account = purple_account_new(username.c_str(), proto.getPurpleID().c_str());

	Account a(account);

	try {
		a.setOptions(options);
	} catch(Protocol::OptionError& e) {
		purple_account_destroy(account);
		throw;
	}

	if (register_account)
		a.registerAccount();

	purple_accounts_add(account);

	const PurpleSavedStatus *saved_status;
	saved_status = purple_savedstatus_get_current();
	if (saved_status != NULL) {
		purple_savedstatus_activate_for_account(saved_status, account);
		purple_account_set_enabled(account, MINBIF_VERSION_NAME, TRUE);
	}

	return a;
}

void Purple::delAccount(PurpleAccount* account)
{
	purple_request_close_with_handle(account); /* Close any other opened delete window */
        purple_accounts_delete(account);
}

}; /* namespace im */
