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

#include <cstring>

#include "irc/irc.h"
#include "irc/chat_buddy.h"
#include "irc/server.h"
#include "im/account.h"
#include "core/util.h"

namespace irc {

ChatBuddy::ChatBuddy(Server* server, im::ChatBuddy _cbuddy)
	: ConvNick(server, im::Conversation(), "","","",""),
	  im_cbuddy(_cbuddy)
{
	string hostname = im_cbuddy.getRealName();
	string identname = stringtok(hostname, "@");
	string nickname = im_cbuddy.getName();
	if(nickname.find('@') != string::npos || nickname.find(' ') != string::npos)
		nickname = nickize(stringtok(nickname, " @"));
	else
		nickname = nickize(nickname);
	if(hostname.empty())
		hostname = im_cbuddy.getAccount().getID();
	else
		hostname += ":" + im_cbuddy.getAccount().getID();

	setNickname(nickname);
	setIdentname(identname);
	setHostname(hostname);
}

ChatBuddy::~ChatBuddy()
{
	im::Conversation conv = getConversation();
	if(conv.isValid() && conv.getNick() == this)
		conv.setNick(NULL);
}

void ChatBuddy::send(Message m)
{
	if(m.getCommand() == MSG_PRIVMSG)
	{
		string text = m.getArg(0);
		if(m.getReceiver() == this)
		{
			if(!getConversation().isValid())
			{
				im::Conversation conv = im::Conversation(im_cbuddy.getAccount(), im_cbuddy);
				conv.present();
				conv.setNick(this);
				setConversation(conv);
			}
			enqueueMessage(text, getServer()->getIRC()->getIM()->getSendDelay());
		}
	}
}

string ChatBuddy::getRealName() const
{
	return im_cbuddy.getName();
}

string ChatBuddy::getAwayMessage() const
{
	/* TODO */
	return "";
}

bool ChatBuddy::isAway() const
{
	/* TODO */
	return false;
}

}; /* namespace irc */
