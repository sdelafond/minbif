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

#ifndef IRC_UNKNOWN_BUDDY_H
#define IRC_UNKNOWN_BUDDY_H

#include "irc/conv_entity.h"

namespace irc
{

	/** This class represents an unknown buddy on IRC */
	class UnknownBuddy : public ConvNick
	{
	public:

		/** Build buddy object
		 *
		 * @param server  up-server
		 * @param conv  conversation object
		 */
		UnknownBuddy(Server* server, im::Conversation& conv);
		~UnknownBuddy();

		/** Implementation of the message routing to this buddy */
		virtual void send(Message m);

		/** Get buddy's away message. */
		virtual string getAwayMessage() const;

		/** Check if user is away or not. */
		virtual bool isAway() const;

		/** Check if buddy is online or not. */
		virtual bool isOnline() const { return true; }

		/** Get buddy's real name. */
		virtual string getRealName() const;

	};

}; /* namespace irc */

#endif /* IRC_UNKNOWN_BUDDY_H */
