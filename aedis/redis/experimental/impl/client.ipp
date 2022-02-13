/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/redis/experimental/client.hpp>
#include <aedis/resp3/detail/parser.hpp>

namespace aedis {
namespace redis {
namespace experimental {

client::client(net::any_io_executor ex)
: socket_{ex}
, timer_{ex}
{
   timer_.expires_at(std::chrono::steady_clock::time_point::max());
}

bool client::prepare_next()
{
   if (std::empty(req_info_)) {
      req_info_.push_back({});
      return true;
   }

   if (req_info_.front().size == 0) {
      // It has already been written and we are waiting for the
      // responses.
      req_info_.push_back({});
      return false;
   }

   return false;
}

bool client::on_read()
{
   assert(!std::empty(req_info_));
   assert(!std::empty(commands_));

   commands_.erase(std::begin(commands_));

   if (--req_info_.front().cmds != 0)
      return false;

   req_info_.erase(std::begin(req_info_));

   return !std::empty(req_info_);
}

} // experimental
} // redis
} // aedis

