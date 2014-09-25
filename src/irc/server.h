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

#ifndef IRC_SERVER_H
#define IRC_SERVER_H

#include <vector>

#include "core/entity.h"
#include "im/account.h"

namespace irc
{
	class IRC;
	class Nick;
	using std::vector;

	/** This class represents an IRC server */
	class Server : public Entity
	{
		string info;
		vector<Nick*> users;

	public:

		/** Build the Server object.
		 *
		 * @param name  server's name.
		 * @param info  string shown to describe this server.
		 */
		Server(string name, string info);

		string getServerName() const { return getName(); }
		string getServerInfo() const { return info; }

		void addNick(Nick* n);
		void removeNick(Nick* n);

		unsigned countNicks() const;
		unsigned countOnlineNicks() const;

		virtual IRC* getIRC() const = 0;
	};

	/** This class represents a remote IRC server.
	 *
	 * A remote IRC server is a server linked to
	 * an IM account.
	 */
	class RemoteServer : public Server
	{
		im::Account account;
		IRC* irc;

	public:

		/** Build the RemoteServer object.
		 *
		 * @param irc  the IRC instance of main server.
		 * @param account  IM account linked to this server.
		 */
		RemoteServer(IRC* irc, im::Account account);

		IRC* getIRC() const { return irc; }

		im::Account getAccount() const { return account; }
	};

}; /* namespace irc */

#endif /* IRC_SERVER_H */
