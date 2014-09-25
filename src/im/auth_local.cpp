/*
 * Minbif - IRC instant messaging gateway
 * Copyright(C) 2010 Romain Bignon, Marc Dequènes (Duck)
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

#include <cstring>
#include <cerrno>
#include "auth.h"
#include "core/log.h"
#include "core/util.h"
#include "core/config.h"
#include "irc/irc.h"
#include "irc/user.h"
#include "auth_local.h"

namespace im
{
AuthLocal::AuthLocal(irc::IRC* _irc, const string& _username)
	: Auth(_irc, _username)
{
}

bool AuthLocal::exists()
{
	return im::IM::exists(username);
}

bool AuthLocal::authenticate(const string& password)
{
	if (!im::IM::exists(username))
		return false;

	im = new im::IM(irc, username);

	b_log[W_DEBUG] << "Authenticating user " << username << " using local database";
	return im->getPassword() == password;
}

bool AuthLocal::setPassword(const string& password)
{
	if(password.find(' ') != string::npos || password.size() < 8)
	{
		irc->notice(irc->getUser(), "Password must be at least 8 characters, and cannot contain whitespaces.");
		return false;
	}
	im->setPassword(password);
	return true;
}

string AuthLocal::getPassword() const
{
	return im->getPassword();
}

im::IM* AuthLocal::create(const string& password)
{
	if (password.empty())
		throw UnableToCreate("Please set a password for this new account");

	return Auth::create(password);
}

}; /* namespace im */
