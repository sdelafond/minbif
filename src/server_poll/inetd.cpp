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

#include <cassert>
#include <glib.h>
#include <unistd.h>

#include "inetd.h"
#include "irc/irc.h"
#include "irc/user.h"
#include "core/callback.h"
#include "core/log.h"
#include "core/minbif.h"
#include "core/util.h"
#include "sockwrap/sockwrap.h"

InetdServerPoll::InetdServerPoll(Minbif* application, ConfigSection* config)
	: ServerPoll(application, config),
	  irc(NULL)
{
	try
	{
		irc = new irc::IRC(this, sock::SockWrapper::Builder(getConfig(), fileno(stdin), fileno(stdout)),
		              conf.GetSection("irc")->GetItem("hostname")->String(),
		              conf.GetSection("irc")->GetItem("ping")->Integer());
#ifndef DEBUG
		/* Don't check if this is a tty, because it always returns false when launched in inetd. */
		if(fileno(stderr) >= 0)
			close(fileno(stderr));
#endif /* DEBUG */
	}
	catch(StrException &e)
	{
		b_log[W_ERR] << "Unable to start the IRC daemon: " + e.Reason();
		throw ServerPollError();
	}
}

InetdServerPoll::~InetdServerPoll()
{
	delete irc;
}

void InetdServerPoll::log(size_t level, string msg) const
{
	string cmd = MSG_NOTICE;
	if(level & W_DEBUG)
		cmd = MSG_PRIVMSG;

	for(string line; (line = stringtok(msg, "\n\r")).empty() == false;)
		irc->getUser()->send(irc::Message(cmd).setSender(irc)
							     .setReceiver(irc->getUser())
							     .addArg(line));
}

void InetdServerPoll::rehash()
{
	if(irc)
		irc->rehash();
}

void InetdServerPoll::kill(irc::IRC* irc)
{
	assert(irc == this->irc);

	_CallBack* stop_cb = new CallBack<InetdServerPoll>(this, &InetdServerPoll::stopServer_cb);
	g_timeout_add(0, g_callback_delete, stop_cb);
}

bool InetdServerPoll::stopServer_cb(void*)
{
	delete irc;
	irc = NULL;

	getApplication()->quit();
	return false;
}
