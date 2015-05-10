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
#include <cstring>
#include "protocol.h"
#include "account.h"
#include "core/util.h"

namespace im {

Protocol::Option::Option()
	: type(Protocol::Option::NONE)
{}

string Protocol::Option::nameFromText(string s) const
{
	FOREACH(string, s, it)
		if(*it == ' ') *it = '-';
		else *it = (char)tolower(*it);
	return s;
}

Protocol::Option::Option(Protocol::Option::type_t _type, string _name, string _text, string _value)
	: type(_type),
	  name(_name),
	  value(_value),
	  text(_text)
{}

Protocol::Option::Option(PurplePrefType _type, string _name, string _text, string _value)
	: name(_name),
	  value(_value),
	  text(_text)
{
	switch(_type)
	{
		case PURPLE_PREF_BOOLEAN:
			type = Protocol::Option::BOOL;
			break;
		case PURPLE_PREF_STRING:
			type = Protocol::Option::STR;
			break;
		case PURPLE_PREF_INT:
			type = Protocol::Option::INT;
			break;
		case PURPLE_PREF_STRING_LIST:
			type = Protocol::Option::STR_LIST;
			break;
		default:
			type = Protocol::Option::NONE;
			break;
	}
}

int Protocol::Option::getValueInt() const
{
	return s2t<int>(value);
}

bool Protocol::Option::getValueBool() const
{
	return (value == "true" || value == "1");
}

bool Protocol::Option::operator==(string s) const
{
	return s == name;
}

Protocol::Protocol()
	: plugin(NULL)
{}

Protocol::Protocol(PurplePlugin* _plugin)
	: plugin(_plugin)
{}

bool Protocol::operator==(const Protocol& proto) const
{
	return proto.getPurpleID() == getPurpleID();
}

bool Protocol::operator!=(const Protocol& proto) const
{
	return proto.getPurpleID() != getPurpleID();
}

string Protocol::getName() const
{
	assert(plugin);
	return plugin->info->name;
}

string Protocol::getID() const
{
	assert(plugin);
	if(!strncmp(plugin->info->id, "prpl-", 5))
		return plugin->info->id + 5;
	else
		return plugin->info->id;
}

string Protocol::getPurpleID() const
{
	assert(plugin);
	return plugin->info->id;
}

Protocol::Options Protocol::getOptions() const
{
	Options options;
	PurplePluginProtocolInfo* prplinfo = PURPLE_PLUGIN_PROTOCOL_INFO(plugin);
	GList *iter;

	for (iter = prplinfo->protocol_options; iter; iter = iter->next)
	{
		PurpleAccountOption *option = static_cast<PurpleAccountOption*>(iter->data);
		PurplePrefType type = purple_account_option_get_type(option);

		Option opt = Option(type,
				    purple_account_option_get_setting(option),
				    purple_account_option_get_text(option));
		switch(type)
		{
			case PURPLE_PREF_BOOLEAN:
				opt.setValue(purple_account_option_get_default_bool(option) ? "true" : "false");
				break;
			case PURPLE_PREF_STRING:
			{
				const char* s = purple_account_option_get_default_string(option);
				if(s && *s)
					opt.setValue(s);
				break;
			}
			case PURPLE_PREF_STRING_LIST:
			{
				const char* s = purple_account_option_get_default_list_value(option);
				vector<string> choices;
				GList* list = purple_account_option_get_list(option);

				if(s && *s)
					opt.setValue(s);

				for(; list; list = list->next)
				{
					PurpleKeyValuePair *kvp = static_cast<PurpleKeyValuePair*>(list->data);
					choices.push_back((const char*)kvp->value);
				}
				opt.setChoices(choices);

				break;
			}
			case PURPLE_PREF_INT:
				opt.setValue(t2s(purple_account_option_get_default_int(option)));
				break;
			default:
				/* This option is not supported */
				continue;
		}
		options[opt.getName()] = opt;
	}
	options["accid"] = Option(Option::ACCID, "accid", "Account ID");
	options["status_channel"] = Option(Option::STATUS_CHANNEL, "status_channel", "Status Channel", "&minbif");
	options["password"] = Option(Option::PASSWORD, "password", "Account password");
	options["server_aliases"] = Option(Option::SERVER_ALIASES, "server_aliases", "Store aliases server-side", "true");
	return options;
}


PurplePluginProtocolInfo* Protocol::getPurpleProtocol() const
{
	return PURPLE_PLUGIN_PROTOCOL_INFO(plugin);
}

}; /* namespace im */
