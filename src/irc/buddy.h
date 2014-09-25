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

#ifndef IRC_BUDDY_H
#define IRC_BUDDY_H

#include "irc/conv_entity.h"
#include "im/buddy.h"

namespace irc
{

	/** This class represents a buddy on IRC */
	class Buddy : public ConvNick
	{
		im::Buddy im_buddy;
		bool public_msgs;
		time_t public_msgs_last;
		static const int PUBLIC_MSGS_TIMEOUT = 3600;

		bool process_dcc_get(const string& text);
		bool received_file(void* data);
		void check_conv(void);

	public:

		/** Build buddy object
		 *
		 * @param server  up-server
		 * @param buddy  IM buddy object.
		 */
		Buddy(Server* server, im::Buddy buddy);
		~Buddy();

		/** Implementation of the message routing to this buddy */
		virtual void send(Message m);

		/** Buddy sends a message to someone. */
		virtual void sendMessage(Nick* to, const string& text, bool action = false);

		/** Get status.
		 * @param away  if false, do not return anything if the buddy is away.
		 */
		virtual string getStatusMessage(bool away = false) const;

		/** Get buddy's away message. */
		virtual string getAwayMessage() const;

		/** Check if user is away or not. */
		virtual bool isAway() const;

		/** Check if buddy is online or not. */
		virtual bool isOnline() const;

		im::Buddy getBuddy() const { return im_buddy; }

		virtual int sendCommand(const string& cmd);

		/** Get icon in an coloured ASCII-art form. */
		virtual CacaImage getIcon() const;

		virtual string getIconPath() const;

		/** Get buddy's real name. */
		virtual string getRealName() const;

		virtual bool retrieveInfo() const;

		virtual void setConversation(const im::Conversation& c);
	};

}; /* namespace irc */

#endif /* IRC_BUDDY_H */
