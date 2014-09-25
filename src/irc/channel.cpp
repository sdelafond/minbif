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

#include "channel.h"
#include "nick.h"
#include "message.h"
#include "irc.h"
#include "core/util.h"

namespace irc {

const char *Channel::CHMODES = "qohvb";

ChanUser::ChanUser(Channel* _chan, Nick* _nick, int _status)
	: Entity(_nick->getNickname()),
	  nick(_nick),
	  chan(_chan),
	  status(_status)
{
}

string ChanUser::getName() const
{
	return nick->getNickname();
}

string ChanUser::getLongName() const
{
	return nick->getLongName();
}

ChanUser::m2c_t ChanUser::m2c[] = {
	{ ChanUser::FOUNDER, 'q', '~' },
	{ ChanUser::OP,      'o', '@' },
	{ ChanUser::HALFOP,  'h', '%' },
	{ ChanUser::VOICE,   'v', '+' },
};

string ChanUser::getPrefix() const
{
	string s;
	size_t i;
	for(i=0; i < sizeof m2c / sizeof *m2c; ++i)
		if(status & m2c[i].mode && m2c[i].prefix != '\0')
			s += m2c[i].prefix;
	return s;
}

ChanUser::mode_t ChanUser::c2mode(char c)
{
	size_t i;
	for(i=0; i < sizeof m2c / sizeof *m2c && m2c[i].c != c; ++i)
		;

	if(i < sizeof m2c / sizeof *m2c)
		return m2c[i].mode;
	else
		return (ChanUser::mode_t)0;
}

char ChanUser::mode2c(ChanUser::mode_t mode)
{
	size_t i;
	for(i=0; i < sizeof m2c / sizeof *m2c && m2c[i].mode != mode; ++i)
		;

	if(i < sizeof m2c / sizeof *m2c)
		return m2c[i].c;
	else
		return 0;
}

Message ChanUser::getModeMessage(bool add, int modes) const
{
	Message message(MSG_MODE);
	string modes_str = add ? "+" : "-";
	size_t i;

	if(!modes)
		modes = this->status;

	message.addArg(modes_str);
	for(i = 0; i < sizeof m2c / sizeof *m2c; ++i)
		if(m2c[i].mode & modes)
		{
			modes_str += m2c[i].c;
			message.addArg(getName());
		}
	message.setArg(0, modes_str);
	return message;
}

Channel::Channel(IRC* _irc, string name)
	: Entity(name),
	  irc(_irc)
{}

Channel::~Channel()
{
	for(vector<ChanUser*>::iterator it = users.begin(); it != users.end(); ++it)
	{
		(*it)->getNick()->send(Message(MSG_PART).setSender(*it)
							.setReceiver(this));
		(*it)->getNick()->removeChanUser(*it);
		delete *it;
	}
	users.clear();
}

void Channel::sendNames(Nick* nick) const
{
	string names;
	for(vector<ChanUser*>::const_iterator it = users.begin(); it != users.end(); ++it)
	{
		names += (*it)->getPrefix();
		names += (*it)->getNick()->getNickname();
		// We're detecting that a space exists before prepending : to arguments.
		// If we don't do it this way, a single-user channel won't prepend the colon to the
		// user list.
		// See Message::format()
		names += " ";
	}

	nick->send(Message(RPL_NAMREPLY).setSender(irc)
			           .setReceiver(nick)
				   .addArg("=")
				   .addArg(getName())
				   .addArg(names));
	nick->send(Message(RPL_ENDOFNAMES).setSender(irc)
			           .setReceiver(nick)
				   .addArg(getName())
				   .addArg("End of /NAMES list"));
}

ChanUser* Channel::addUser(Nick* nick, int status)
{
	ChanUser* chanuser = new ChanUser(this, nick, status);
	users.push_back(chanuser);

	string names;
	for(vector<ChanUser*>::iterator it = users.begin(); it != users.end(); ++it)
	{
		(*it)->getNick()->send(Message(MSG_JOIN).setSender(nick).setReceiver(this));
		if(status && (*it)->getNick() != nick)
		{
			Message m = chanuser->getModeMessage(true);
			m.setSender(irc);
			m.setReceiver(this);
			(*it)->getNick()->send(m);
		}
	}
	string topic = getTopic();
	if(!topic.empty())
		nick->send(Message(RPL_TOPIC).setSender(irc)
					     .setReceiver(nick)
					     .addArg(getName())
					     .addArg(topic));

	sendNames(nick);
	return chanuser;
}

void Channel::delUser(Nick* nick, Message m)
{
	for(vector<ChanUser*>::iterator it = users.begin(); it != users.end(); )
		if((*it)->getNick() == nick)
		{
			delete *it;
			it = users.erase(it);
		}
		else
		{
			if(m.getCommand().empty() == false)
				(*it)->getNick()->send(m);
			++it;
		}
}

ChanUser* Channel::getChanUser(string nick) const
{
	/* Match is case sensitive */
	nick = strlower(nick);
	for(vector<ChanUser*>::const_iterator it = users.begin(); it != users.end(); ++it)
		if(strlower((*it)->getNick()->getNickname()) == nick)
			return *it;
	return NULL;
}

void Channel::broadcast(Message m, Nick* butone)
{
	for(vector<ChanUser*>::iterator it = users.begin(); it != users.end(); ++it)
		if(!butone || (*it)->getNick() != butone)
			(*it)->getNick()->send(m);
}

void Channel::m_mode(Nick* user, Message m)
{
	if(m.countArgs() == 0)
	{
		user->send(Message(RPL_CHANNELMODEIS).setSender(irc)
						     .setReceiver(user)
						     .addArg(getName())
						     .addArg("+"));
		user->send(Message(RPL_CREATIONTIME).setSender(irc)
						     .setReceiver(user)
						     .addArg(getName())
						     .addArg("1212313"));
		return;
	}
	vector<string> args = m.getArgs();
	vector<string>::iterator arg = args.begin();
	string modes = *arg++;
	bool add = true;

	FOREACH(string, modes, c)
	{
		switch(*c)
		{
			case '+':
				add = true;
				break;
			case '-':
				add = false;
				break;
			case 'b':
				if(arg == args.end())
					showBanList(user);
				else
					processBan(user, *arg++, add);
				break;
			default:
				break;
		}
	}

}

void Channel::setMode(const Entity* sender, int modes, ChanUser* chanuser)
{
	if(!modes) return;
	chanuser->setStatus(modes);
	Message m = chanuser->getModeMessage(true, modes);
	m.setSender(sender ? sender : irc);
	m.setReceiver(this);
	broadcast(m);
}

void Channel::delMode(const Entity* sender, int modes, ChanUser* chanuser)
{
	if(!modes) return;
	chanuser->delStatus(modes);
	Message m = chanuser->getModeMessage(false, modes);
	m.setSender(sender ? sender : irc);
	m.setReceiver(this);
	broadcast(m);

}

bool Channel::setTopic(Entity* chanuser, const string& topic)
{
	if (this->topic == topic)
		return true;

	this->topic = topic;
	broadcast(Message(MSG_TOPIC).setSender(chanuser ? chanuser : irc)
			            .setReceiver(this)
				    .addArg(topic));
	return true;
}

}; /* namespace irc */
