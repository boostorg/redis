/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/sentinel/command.hpp>

namespace aedis {
namespace sentinel {

char const* to_string(command c)
{
   static char const* table[] = {
      "ACL",
      "AUTH",
      "CLIENT",
      "COMMAND",
      "HELLO",
      "INFO",
      "PING",
      "PSUBSCRIBE",
      "PUBLISH",
      "PUNSUBSCRIBE",
      "ROLE",
      "SENTINEL",
      "SHUTDOWN",
      "SUBSCRIBE",
      "UNSUBSCRIBE",
   };

   return table[static_cast<int>(c)];
}

std::ostream& operator<<(std::ostream& os, command c)
{
   os << to_string(c);
   return os;
}

bool has_push_response(command cmd)
{
   switch (cmd) {
      case command::subscribe:
      case command::unsubscribe:
      case command::psubscribe:
      return true;

      default:
      return false;
   }
}

} // sentinel
} // aedis
