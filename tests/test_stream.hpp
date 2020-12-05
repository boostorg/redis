/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/aedis.hpp>

namespace aedis {

struct initiate_async_receive {
   using executor_type = aedis::net::system_executor;

   std::string& payload;

   executor_type get_executor() noexcept
      { return aedis::net::system_executor(); }

   template <
      class ReadHandler,
      class MutableBufferSequence>
   void operator()(
      ReadHandler&& handler,
      MutableBufferSequence const& buffers) const
   {
      boost::system::error_code ec;
      if (std::size(buffers) == 0) {
        handler(ec, 0);
        return;
      }

      auto begin = boost::asio::buffer_sequence_begin(buffers);
      assert(std::size(payload) <= std::size(*begin));
      char* p = static_cast<char*>(begin->data());
      std::copy(std::begin(payload), std::end(payload), p);
      handler(ec, std::size(payload));
   }
};

template <class Executor>
struct test_stream {
   std::string payload;

   using executor_type = Executor;

   template<
      class MutableBufferSequence,
      class ReadHandler =
          aedis::net::default_completion_token_t<executor_type>
    >
    auto async_read_some(
        MutableBufferSequence const& buffers,
        ReadHandler&& handler = net::default_completion_token_t<executor_type>{})
    {
      return aedis::net::async_initiate<ReadHandler,
        void (boost::system::error_code, std::size_t)>(
          initiate_async_receive{payload}, handler, buffers);
    }

    executor_type get_executor() noexcept
       { return aedis::net::system_executor(); }

     template<class Executor1>
     struct rebind_executor {
	using other = test_stream<Executor1>;
     };
};

}
