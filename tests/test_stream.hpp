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

   std::string::const_iterator pbegin;
   std::string::const_iterator pend;

   executor_type get_executor() noexcept
      { return aedis::net::system_executor(); }

   template <
      class ReadHandler,
      class MutableBufferSequence>
   void operator()(
      ReadHandler&& handler,
      MutableBufferSequence const& buffers)
   {
      boost::system::error_code ec;
      if (std::size(buffers) == 0) {
        handler(ec, 0);
        return;
      }

      auto begin = boost::asio::buffer_sequence_begin(buffers);
      auto end = boost::asio::buffer_sequence_end(buffers);
      //std::cout << "Buffers size: " << std::size(buffers) << std::endl;

      std::size_t transferred = 0;
      while (begin != end) {
        //std::cout << "Buffer size: " << std::ssize(*begin) << std::endl;
        auto const min = std::min(std::ssize(*begin), pend - pbegin);
        std::copy(pbegin, pbegin + min, static_cast<char*>(begin->data()));
        std::advance(pbegin, min);
        transferred += min;
        ++begin;
      }

      handler(ec, transferred);
   }
};

template <class Executor>
struct test_stream {
   std::string const payload;

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
          initiate_async_receive
          {std::cbegin(payload), std::cend(payload)},
           handler, buffers);
    }

   template<class MutableBufferSequence>
   std::size_t read_some(
      MutableBufferSequence const& buffers,
      boost::system::error_code&)
   {
      if (std::size(buffers) == 0)
        return 0;

      boost::system::error_code ec;
      auto pbegin = std::cbegin(payload);
      auto pend = std::cend(payload);

      auto begin = boost::asio::buffer_sequence_begin(buffers);
      auto end = boost::asio::buffer_sequence_end(buffers);

      std::size_t transferred = 0;
      while (begin != end) {
        auto const min = std::min(std::ssize(*begin), pend - pbegin);
        std::copy(pbegin, pbegin + min, static_cast<char*>(begin->data()));
        std::advance(pbegin, min);
        transferred += min;
        ++begin;
      }

      return transferred;
   }

   executor_type get_executor() noexcept
      { return aedis::net::system_executor(); }

     template<class Executor1>
     struct rebind_executor {
	using other = test_stream<Executor1>;
     };
};

}
