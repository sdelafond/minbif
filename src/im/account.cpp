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

#include <cassert>

#include "account.h"
#include "buddy.h"
#include "purple.h"
#include "../log.h"
#include "../version.h"
#include "im.h"
#include "irc/irc.h"
#include "irc/channel.h"

namespace im {

Account::Account()
	: account(NULL)
{}

Account::Account(PurpleAccount* _account, Protocol _proto)
	: account(_account),
	  proto(_proto)
{
	if(!proto.isValid())
		proto = Purple::getProtocolByPurpleID(account->protocol_id);
}

bool Account::operator==(const Account& account) const
{
	return this->isValid() && account.isValid() && account.account == this->account;
}

bool Account::operator!=(const Account& account) const
{
	return !isValid() || !account.isValid() || account.account != this->account;
}


string Account::getUsername() const
{
	assert(account);
	return account->username;
}

string Account::getID() const
{
	string n = purple_account_get_ui_string(account, BITLBEE_VERSION_NAME, "id", "");
	if(n.empty())
		n = Purple::getNewAccountName(proto);
	return n;
}

string Account::getStatusChannel() const
{
	string n = purple_account_get_ui_string(account, BITLBEE_VERSION_NAME, "channel", "");
	return n;
}

void Account::setStatusChannel(string c)
{
	purple_account_set_ui_string(account, BITLBEE_VERSION_NAME, "channel", c.c_str());
}

string Account::getServername() const
{
	return getUsername() + ":" + getID();
}

bool Account::isConnected() const
{
	return purple_account_is_connected(account);
}

bool Account::isConnecting() const
{
	return purple_account_is_connecting(account);
}

vector<Buddy> Account::getBuddies() const
{
	vector<Buddy> buddies;
	return buddies;
}

void Account::connect() const
{
	purple_account_set_enabled(account, BITLBEE_VERSION_NAME, true);
}

void Account::disconnect() const
{
	purple_account_set_enabled(account, BITLBEE_VERSION_NAME, false);
}

void Account::createStatusChannel() const
{
	irc::IRC* irc = Purple::getIM()->getIRC();
	irc::Channel* chan;
	string channame = getStatusChannel();

	if(!irc::Channel::isStatusChannel(channame))
		return;

	chan = irc->getChannel(channame);
	if(!chan)
	{
		chan = new irc::Channel(irc, channame);
		irc->addChannel(chan);
	}
}

/* STATIC */

PurpleConnectionUiOps Account::conn_ops =
{
	Account::connecting,
        Account::connected,
        Account::disconnected,
        NULL, /* notice */
        NULL,
        NULL, /* network_connected */
        NULL, /* network_disconnected */
        Account::disconnect_reason,
        NULL,
        NULL,
        NULL
};


void* Account::getHandler()
{
	static int handler;

	return &handler;
}

void Account::init()
{
	purple_connections_set_ui_ops(&conn_ops);
	purple_signal_connect(purple_accounts_get_handle(), "account-added",
				getHandler(), PURPLE_CALLBACK(account_added),
				NULL);
	purple_signal_connect(purple_accounts_get_handle(), "account-removed",
				getHandler(), PURPLE_CALLBACK(account_removed),
				NULL);

	map<string, Account> accounts = Purple::getAccountsList();
	for(map<string, Account>::iterator it = accounts.begin(); it != accounts.end(); ++it)
		it->second.createStatusChannel();
}

void Account::account_added(PurpleAccount* account)
{
}

void Account::account_removed(PurpleAccount* account)
{
}

void Account::connecting(PurpleConnection *gc,
		const char *text,
		size_t step,
		size_t step_count)
{
	Account account = Account(gc->account);

	if(!step)
		b_log[W_INFO|W_SNO] << "Connection to " << account.getServername() << " in progress...";
	else
		b_log[W_INFO|W_SNO] << "" << account.getID() << "(" << step << "/" << step_count-1 << "): " << text;
}

void Account::connected(PurpleConnection* gc)
{
	Account account = Account(gc->account);
	irc::IRC* irc = Purple::getIM()->getIRC();

	b_log[W_INFO|W_SNO] << "Connection to " << account.getServername() << " established!";
	irc->addServer(new irc::RemoteServer(irc, account));
}

void Account::disconnected(PurpleConnection* gc)
{
	Account account = Account(gc->account);

	b_log[W_INFO|W_SNO] << "Closing link with " << account.getServername();
	Purple::getIM()->getIRC()->removeServer(account.getServername());
}

void Account::disconnect_reason(PurpleConnection *gc,
				PurpleConnectionError reason,
				const char *text)
{
	b_log[W_ERR|W_SNO] << "Error(" << Account(gc->account).getServername() << "): " << text;
}

}; /* namespace im */