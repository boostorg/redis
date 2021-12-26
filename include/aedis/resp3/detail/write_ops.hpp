/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/net.hpp>
#include <boost/asio/yield.hpp>
#include <boost/core/ignore_unused.hpp>

namespace aedis {
namespace resp3 {
namespace detail {

template<
   class AsyncWriteStream,
   class Queue
>
struct write_some_op {
   AsyncWriteStream& stream;
   Queue& reqs;
   std::size_t total_writen_ = 0;
   net::coroutine coro_ = net::coroutine();

   void
   operator()(
      auto& self,
      boost::system::error_code const& ec = {},
      std::size_t n = 0)
   {
      boost::ignore_unused(n);

      reenter (coro_) {
	 do {
	    assert(!std::empty(reqs));
	    assert(!std::empty(reqs.front().request()));

	    yield net::async_write(
	       stream,
	       net::buffer(reqs.front().request()),
	       std::move(self));

	    if (ec)
	       break;

	    total_writen_ += n;

            // Pops the request if no response is expected.
	    if (std::empty(reqs.front().commands))
	       reqs.pop();

	 } while (!std::empty(reqs) && std::empty(reqs.front().commands));

         self.complete(ec, total_writen_);
      }
   }
};

} // detail
} // resp3
} // aedis

#include <boost/asio/unyield.hpp>
