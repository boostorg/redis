/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <aedis/command.hpp>

namespace aedis {

bool has_push_response(boost::string_view cmd)
{
   if (cmd == "SUBSCRIBE") return true;
   if (cmd == "PSUBSCRIBE") return true;
   if (cmd == "UNSUBSCRIBE") return true;
   return false;
}

} // aedis
