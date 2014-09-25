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

#ifndef IM_ACCOUNT_H
#define IM_ACCOUNT_H

#include <purple.h>
#include <string>
#include <map>
#include <vector>

#include "im/protocol.h"

namespace irc
{
	class StatusChannel;
}

namespace im
{
	using std::string;
	using std::vector;
	using std::map;
	class Buddy;

	/** This class represents an account.
	 *
	 * This class only interfaces between the minbif code
	 * and a libpurple account object.
	 */
	class Account
	{
		PurpleAccount* account;
		Protocol proto;

		static PurpleConnectionUiOps conn_ops;
		static PurpleAccountUiOps acc_ops;
		static void* getHandler();
		static void account_signed_on_cb(PurpleConnection *gc, gpointer event);
		static void account_added(PurpleAccount*);
		static void account_removed(PurpleAccount*);
		static void notify_added(PurpleAccount *account, const char *remote_user,
						const char *id, const char *alias,
						const char *msg);
		static void request_add(PurpleAccount *account, const char *remote_user,
					  const char *id, const char *alias,
					  const char *msg);
		static void *request_authorize(PurpleAccount *account,
					const char *remote_user,
					const char *id,
					const char *alias,
					const char *message,
					gboolean on_list,
					PurpleAccountRequestAuthorizationCb auth_cb,
					PurpleAccountRequestAuthorizationCb deny_cb,
					void *user_data);
		static void request_close(void *uihandle);
		static void connecting(PurpleConnection *gc,
		                       const char *text,
		                       size_t step,
		                       size_t step_count);
		static void connected(PurpleConnection* gc);
		static void disconnected(PurpleConnection* gc);
		static void disconnect_reason(PurpleConnection *gc,
		                              PurpleConnectionError reason,
		                              const char *text);
		static gboolean reconnect(void*);

	public:

		/** Initialization of libpurple accounts' stuffs. */
		static void init();
		static void uninit();

		/** Empty constructor */
		Account();

		/** Create an account instance.
		 *
		 * @param account  the libpurple's account object.
		 * @param proto  optional argument to provide the protocol object.
		 */
		Account(PurpleAccount* account, Protocol proto = Protocol());

		/** Comparaison between two accounts */
		bool operator==(const Account&) const;
		bool operator!=(const Account&) const;

		bool isValid() const { return account != NULL; }
		PurpleAccount* getPurpleAccount() const { return account; }
		PurpleConnection* getPurpleConnection() const;

		/** Get ID of account.
		 *
		 * @param calculate_newone  if true, when the ID is not set, calculate a new one.
		 * @return  a string un form \a "<protocol><number>"
		 */
		string getID(bool calculate_newone = true) const;

		void setID(string id) const;

		/** Get username of this account */
		string getUsername() const;

		/** Get password of this account */
		string getPassword() const;
		void setPassword(string password);

		/** Enable/Disable store of aliases server-side. */
		void setServerAliases(bool b);
		bool hasServerAliases() const;

		/** Register account on server. */
		void registerAccount() const;

		Protocol::Options getOptions() const;
		void setOptions(const Protocol::Options& options);

		/** Set path to the buddy icon to use for this account. */
		void setBuddyIcon(string filename);

		/** Set status */
		void setStatus(PurpleStatusPrimitive pri, string message = "");

		/** Get status */
		PurpleStatusPrimitive getStatus() const;

		/** Get status message */
		string getStatusMessage(PurpleStatusPrimitive pri = PURPLE_STATUS_UNSET) const;

		/** Get room list from this account.
		 * The roomlist callbacks will be called.
		 */
		void displayRoomList() const;

		/** Get name of IRC server linked to this account.
		 *
		 * @return  a string in form \a "<username>:<protocol><number>"
		 */
		string getServername() const;

		/** Get status channel name */
		string getStatusChannelName() const;

		/** Set status channel name */
		void setStatusChannelName(const string& c);

		/** Get the Status Channel instance. */
		irc::StatusChannel* getStatusChannel() const;

		/** Enqueue a channel name to join at connection. */
		void enqueueChannelJoin(const string& c);

		/** Try to join every channels in queue.
		 * It also remove them from queue.
		 */
		void flushChannelJoins();

		/** Clear the channel joins queue without trying to join them. */
		void abortChannelJoins();

		Protocol getProtocol() const { return proto; }
		bool isConnected() const;
		bool isConnecting() const;

		/** Auto reconnect to this account with a delay. */
		int delayReconnect() const;

		/** Abort the auto-reconnection. */
		void removeReconnection(bool verbose = false) const;

		/** Get list of available commands. */
		vector<string> getCommandsList() const;

		/** Call a command. */
		bool callCommand(const string& command) const;

		/** \todo TODO implement it */
		vector<Buddy> getBuddies() const;

		/** All buddiers are updated */
		void updatedAllBuddies() const;

		/** Connect account */
		void connect() const;

		/** Disconnect account */
		void disconnect() const;

		/** Create the status channel on the IRC network */
		void createStatusChannel();

		/** Leave the status channel */
		void leaveStatusChannel();

		/** Get list of denied people */
		vector<string> getDenyList() const;

		/** Deny a user. */
		bool deny(const string& who) const;

		/** Allow a user. */
		bool allow(const string& who) const;

		/** Add a buddy on this account
		 *
		 * @param username  user name
		 * @param group  group name
		 */
		void addBuddy(const string& username, const string& group) const;

		/** Remove a buddy from this account
		 *
		 * @param buddy  Buddy's instance
		 */
		void removeBuddy(Buddy buddy) const;

		/** Does this account support chats? */
		bool supportsChats() const;

		/** Join a chat */
		bool joinChat(const string& name, const string& parameters) const;

		/** Get parameters list */
		map<string, string> getChatParameters() const;
	};
};

#endif /* IM_ACCOUNT_H */
