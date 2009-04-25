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

#ifndef IRC_NICK_H
#define IRC_NICK_H

#include <map>

#include "entity.h"
#include "message.h"

class CacaImage;

namespace irc
{
	class Server;
	class Channel;
	class ChanUser;

	using std::map;

	class Nick : public Entity
	{
		string identname, hostname, realname;
		string away;
		Server* server;
		unsigned int flags;
		vector<ChanUser*> channels;

	public:

		static const char *nick_lc_chars;
		static const char *nick_uc_chars;
		static const size_t MAX_LENGTH = 29;

		enum {
			REGISTERED = 1 << 0,
			PING       = 1 << 1,
		};

		Nick(Server* server, string nickname, string identname, string hostname, string realname="");
		virtual ~Nick();

		static bool isValidNickname(const string& n);

		virtual void send(Message m) {}

		void join(Channel* chan, int status = 0);
		void part(Channel* chan, string message="");
		void quit(string message="");

		void privmsg(Channel* chan, string message);
		void privmsg(Nick* to, string message);

		vector<ChanUser*> getChannels() const;
		bool isOn(const Channel* chan) const;
		ChanUser* getChanUser(const Channel* chan) const;

		Server* getServer() const { return server; }

		string getLongName() const;

		string getNickname() const { return getName(); }
		void setNickname(string n) { setName(n); }

		string getIdentname() const { return identname; }
		void setIdentname(string i) { identname = i; }

		string getHostname() const { return hostname; }
		void setHostname(string h) { hostname = h; }

		virtual string getRealname() const { return realname; }
		void setRealname(string r) { realname = r; }

		void setFlag(unsigned flag) { flags |= flag; }
		void delFlag(unsigned flag) { flags &= ~flag; }
		bool hasFlag(unsigned flag) const { return flags & flag; }

		void setAwayMessage(string a) { away = a; }
		virtual string getAwayMessage() const { return away; }
		virtual bool isAway() const { return away.empty() == false; }
		virtual bool isOnline() const { return true; }

		virtual CacaImage getIcon() const;
	};

}; /* namespace irc */

#endif /* IRC_NICK_H */
