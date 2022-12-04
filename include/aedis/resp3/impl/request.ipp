/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <string_view>
#include <aedis/resp3/request.hpp>

namespace aedis::resp3::detail {

auto has_push_response(std::string_view cmd) -> bool
{
   if (cmd == "SUBSCRIBE") return true;
   if (cmd == "PSUBSCRIBE") return true;
   if (cmd == "UNSUBSCRIBE") return true;
   return false;
}

auto is_hello(std::string_view cmd) -> bool
{
   return cmd == "HELLO";
}

} // aedis::resp3::detail
