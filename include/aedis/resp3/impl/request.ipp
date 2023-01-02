/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <aedis/resp3/request.hpp>

#include <string_view>

namespace aedis::resp3::detail {

auto has_response(std::string_view cmd) -> bool
{
   if (cmd == "SUBSCRIBE") return true;
   if (cmd == "PSUBSCRIBE") return true;
   if (cmd == "UNSUBSCRIBE") return true;
   return false;
}

} // aedis::resp3::detail
