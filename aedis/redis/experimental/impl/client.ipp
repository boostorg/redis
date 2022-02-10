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

net::awaitable<void> client::writer()
{
   boost::system::error_code ec;
   while (socket_.is_open()) {
      // TODO: Limit the size of the pipelines by spliting that last
      // one in two if needed.
      if (!std::empty(req_info_)) {
         assert(req_info_.front().size != 0);
         assert(!std::empty(requests_));
         ec = {};
         co_await net::async_write(
            socket_, net::buffer(requests_.data(), req_info_.front().size),
            net::redirect_error(net::use_awaitable, ec));
         if (ec) {
           // What should we do here exactly? Closing the socket will
           // cause the reader coroutine to return so that the engage
           // coroutine returns to the user.
           socket_.close();
           co_return;
         }

         requests_.erase(0, req_info_.front().size);
         req_info_.front().size = 0;
         
         if (req_info_.front().cmds == 0) 
            req_info_.pop();
      }

      ec = {};
      co_await timer_.async_wait(net::redirect_error(net::use_awaitable, ec));
      if (stop_writer_)
         co_return;
   }
}

bool client::prepare_next()
{
   if (std::empty(req_info_)) {
      req_info_.push({});
      return true;
   }

   if (req_info_.front().size == 0) {
      // It has already been written and we are waiting for the
      // responses.
      req_info_.push({});
      return false;
   }

   return false;
}

} // experimental
} // redis
} // aedis

