/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "responses.hpp"

namespace net = aedis::net;
using aedis::command;
using aedis::resp3::adapt;
using aedis::resp3::response_traits;
using aedis::resp3::type;

adapter_wrapper::adapter_wrapper(responses& resps)
: number_adapter_{adapt(resps.number)}
, str_adapter_{adapt(resps.simple_string)}
, general_adapter_{adapt(resps.general)}
{}

void adapter_wrapper::operator()(
   command cmd,
   type t,
   std::size_t aggregate_size,
   std::size_t depth,
   char const* data,
   std::size_t size,
   std::error_code& ec)
{
   // Handles only the commands we are interested in the examples and
   // ignores the rest.
   switch (cmd) {
      case command::ping: str_adapter_(t, aggregate_size, depth, data, size, ec); return;
      case command::incr: number_adapter_(t, aggregate_size, depth, data, size, ec); return;
      case command::unknown: general_adapter_(t, aggregate_size, depth, data, size, ec); return;
      default: {} // Ignore.
   }
}
