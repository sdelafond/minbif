/*
 * Minbif - IRC instant messaging gateway
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

#include "conversation_channel.h"
#include "chat_buddy.h"
#include "nick.h"
#include "user.h"
#include "irc.h"
#include "core/log.h"

namespace irc {

ConversationChannel::ConversationChannel(IRC* irc, const im::Conversation& conv)
	: Channel(irc, conv.getChanName()),
	  ConvEntity(conv),
	  upserver(NULL)
{

	upserver = dynamic_cast<RemoteServer*>(irc->getServer(conv.getAccount().getServername()));
}

ConversationChannel::~ConversationChannel()
{
	map<im::ChatBuddy, ChanUser*>::iterator it;
	for(it = cbuddies.begin(); it != cbuddies.end(); ++it)
	{
		irc::ChatBuddy* cb = dynamic_cast<irc::ChatBuddy*>(it->second->getNick());

		if(cb)
		{
			/* Call the Channel function to not erase chatbuddy from cbuddies,
			 * because I iterate on.
			 * I know that Channel destructor will also remove chanuser from list,
			 * but it may also call some Nick methods, as the object is deleted
			 * by IRC::removeNick()...
			 */
			Channel::delUser(cb);
			cb->removeChanUser(it->second);
			irc->removeNick(cb->getNickname());
		}
	}
}

void ConversationChannel::showBanList(Nick* to)
{
	to->send(Message(RPL_ENDOFBANLIST).setSender(irc)
			                  .setReceiver(to)
					  .addArg(getName())
					  .addArg("End of Channel Ban List"));
}

void ConversationChannel::processBan(Nick* from, string pattern, bool add)
{

}

void ConversationChannel::addBuddy(im::ChatBuddy cbuddy, int status)
{
	ChanUser* cul;
	if(cbuddy.isMe())
		cul = irc->getUser()->join(this, status);
	else
	{
		map<im::ChatBuddy, ChanUser*>::iterator it = cbuddies.find(cbuddy);
		if(it == cbuddies.end())
		{
			ChatBuddy* n = new ChatBuddy(upserver, cbuddy);
			while(irc->getNick(n->getNickname()))
				n->setNickname(n->getNickname() + "_");

			irc->addNick(n);
			cul = n->join(this, status);
		}
		else
			cul = it->second;
	}

	cbuddies[cbuddy] = cul;
}

void ConversationChannel::updateBuddy(im::ChatBuddy cbuddy)
{
	ChanUser* chanuser = getChanUser(cbuddy);
	int mlast = chanuser->getStatus();
	int mnew = cbuddy.getChanStatus();
	int add = 0, del = 0;
	for(unsigned i = 0; i < (sizeof i) * 8; ++i)
	{
		if(!(mlast & (1 << i)) && mnew & (1 << i))
			add |= 1 << i;
		if(mlast & (1 << i) && !(mnew & (1 << i)))
			del |= 1 << i;
	}
	if(add)
		this->setMode(irc, add, chanuser);
	if(del)
		this->delMode(irc, del, chanuser);
}

void ConversationChannel::renameBuddy(ChanUser* chanuser, im::ChatBuddy cbuddy)
{
	ChatBuddy* nick = dynamic_cast<irc::ChatBuddy*>(chanuser->getNick());

	string new_nick = cbuddy.getName();
	while(irc->getNick(new_nick))
		new_nick += "_";
	irc->getUser()->send(irc::Message(MSG_NICK).setSender(nick)
			                           .addArg(new_nick));
	irc->renameNick(nick, new_nick);

	cbuddies.erase(nick->getChatBuddy());
	nick->setChatBuddy(cbuddy);
	cbuddies[cbuddy] = chanuser;
}

void ConversationChannel::delUser(Nick* nick, Message message)
{
	map<im::ChatBuddy, ChanUser*>::iterator it;
	for(it = cbuddies.begin(); it != cbuddies.end() && it->second->getNick() != nick; ++it)
		;

	if(it != cbuddies.end())
		cbuddies.erase(it);

	Channel::delUser(nick, message);

	irc::ChatBuddy* cb = dynamic_cast<irc::ChatBuddy*>(nick);
	if(cb)
		irc->removeNick(cb->getNickname());
	else if(nick == irc->getUser())
		getConversation().leave();
}

ChanUser* ConversationChannel::getChanUser(string nick) const
{
	map<im::ChatBuddy, ChanUser*>::const_iterator it;
	nick = strlower(nick);
	for(it = cbuddies.begin(); it != cbuddies.end(); ++it)
		if(strlower(it->first.getName()) == nick)
			return it->second;
	return NULL;
}

ChanUser* ConversationChannel::getChanUser(const im::ChatBuddy& cb) const
{
	map<im::ChatBuddy, ChanUser*>::const_iterator it = cbuddies.find(cb);
	if(it != cbuddies.end())
		return it->second;
	else
		return NULL;
}

void ConversationChannel::broadcast(Message m, Nick* butone)
{
	if(m.getCommand() == MSG_PRIVMSG && m.getSender() == irc->getUser())
	{
		enqueueMessage(m.getArg(0), irc->getIM()->getSendDelay());
	}
	else
		Channel::broadcast(m, butone);
}

string ConversationChannel::getTopic() const
{
	return getConversation().getChanTopic();
}

bool ConversationChannel::invite(Nick* nick, const string& buddy, const string& message)
{
	getConversation().invite(buddy, message);
	return true;
}

bool ConversationChannel::kick(ChanUser* from, ChanUser* victim, const string& message)
{
	/* TODO implement it */
	return false;
}

bool ConversationChannel::setTopic(Entity* from, const string& topic)
{
	if(irc->getUser() == from)
		return getConversation().setTopic(topic);
	else
		return Channel::setTopic(from, topic);
}

int ConversationChannel::sendCommand(const string& cmd)
{
	return getConversation().sendCommand(cmd);
}

}; /* namespace irc */
