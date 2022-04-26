/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
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
