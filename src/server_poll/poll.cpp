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

#include "poll.h"
#include "inetd.h"
#include "daemon_fork.h"
#include "core/log.h"

ServerPoll* ServerPoll::build(ServerPoll::poll_type_t type, Minbif* application)
{
	ConfigSection* config;

	switch(type)
	{
		case ServerPoll::INETD:
			config = conf.GetSection("irc")->GetSection("inetd");
			return new InetdServerPoll(application, config);
		case ServerPoll::DAEMON_FORK:
			config = conf.GetSection("irc")->GetSection("daemon");
			return new DaemonForkServerPoll(application, config);
		case ServerPoll::DAEMON:
		default:
			b_log[W_ERR] << "Type " << type << " is not implemented yet.";
	}

	throw ServerPollError();
}

ServerPoll::ServerPoll(Minbif* _app, ConfigSection* _config)
	: application(_app), config(_config)
{}
