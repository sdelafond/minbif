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

#include <sys/socket.h>
#include <fcntl.h>
#include <netdb.h>
#include <cstring>

#include "../log.h"
#include "../util.h"
#include "../callback.h"
#include "irc.h"
#include "message.h"
#include "nick.h"

static struct
{
	const char* cmd;
	void (IRC::*func)(Message);
} commands[] = {
	{ "NICK", &IRC::m_nick }
};

IRC::IRC(int _fd, string _hostname)
	: fd(_fd),
	  read_cb(NULL),
	  hostname("localhost.localdomain"),
	  userNick(NULL)
{
	struct sockaddr_storage sock;
	socklen_t socklen = sizeof(sock);

	fcntl(0, F_SETFL, O_NONBLOCK);

	/* create a callback on the sock. */
	read_cb = new CallBack<IRC>(this, &IRC::readIO);
	glib_input_add(0, (PurpleInputCondition)PURPLE_INPUT_READ, g_callback, read_cb);

	/* Get the user's hostname. */
	string userhost = "localhost.localdomain";
	if(getpeername(fd, (struct sockaddr*) &sock, &socklen) == 0)
	{
		char buf[NI_MAXHOST+1];

		if(getnameinfo((struct sockaddr *)&sock, socklen, buf, NI_MAXHOST, NULL, 0, 0) == 0)
			userhost = buf;
	}

	userNick = new Nick("AUTH", "auth", userhost);

	/* Get my own hostname (if not given in arguments) */
	if(_hostname.empty() || _hostname == " ")
	{
		if(getsockname(fd, (struct sockaddr*) &sock, &socklen) == 0)
		{
			char buf[NI_MAXHOST+1];

			if(getnameinfo((struct sockaddr *) &sock, socklen, buf, NI_MAXHOST, NULL, 0, 0 ) == 0)
				hostname = buf;
		}
	}
	else if(_hostname.find(" ") != string::npos)
	{
		/* An hostname can't contain any space. */
		b_log[W_ERR] << "'" << _hostname << "' is not a valid server hostname";
		throw IRCAuthError();
	}
	else
		hostname = _hostname;

	sendmsg(Message(MSG_NOTICE).setSender(this).setReceiver(userNick).addArg("BitlBee-IRCd initialized, please go on"));
}

IRC::~IRC()
{
	delete read_cb;
	delete userNick;
}

bool IRC::sendmsg(Message msg) const
{
	string s = msg.format();
	write(fd, s.c_str(), s.size());

	return true;
}

void IRC::readIO(void*)
{
	static char line[1024];
	ssize_t st;

	st = read( 0, line, sizeof( line ) - 1 );
	line[st] = 0;

	Message m = Message::parse(line);
	for(size_t i = 0; i < (sizeof commands / sizeof *commands); ++i)
		if(!strcmp(commands[i].cmd, m.getCommand().c_str()))
		{
			(this->*commands[i].func)(m);
			return;
		}
}

/* NICK nickname */
void IRC::m_nick(Message message)
{
	if(message.countArgs() < 1)
	{
		sendmsg(Message(RPL_NONICKNAMEGIVEN).setSender(this).setReceiver(userNick).addArg("No nickname given"));
		return;
	}
	if(userNick->hasFlag(Nick::REGISTERED))
		sendmsg(Message(MSG_NICK).setSender(userNick).setReceiver(message.getArg(0)));

	userNick->setNickname(message.getArg(0));
}
