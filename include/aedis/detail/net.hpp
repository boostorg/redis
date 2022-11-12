/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_NET_HPP
#define AEDIS_NET_HPP

#include <array>

#include <boost/system.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/connect.hpp>
#include <boost/assert.hpp>
#include <boost/asio/experimental/parallel_group.hpp>

#include <boost/asio/yield.hpp>

namespace aedis::detail {

template <class Channel>
struct send_receive_op {
   Channel* channel;
   boost::asio::coroutine coro{};

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , std::size_t = 0)
   {
      reenter (coro)
      {
         yield
         channel->async_send(boost::system::error_code{}, 0, std::move(self));
         AEDIS_CHECK_OP1();

         yield
         channel->async_receive(std::move(self));
         AEDIS_CHECK_OP1();

         self.complete({}, 0);
      }
   }
};

template <
   class Channel,
   class CompletionToken =
      boost::asio::default_completion_token_t<typename Channel::executor_type>
   >
auto async_send_receive(Channel& channel, CompletionToken&& token = CompletionToken{})
{
   return boost::asio::async_compose
      < CompletionToken
      , void(boost::system::error_code, std::size_t)
      >(send_receive_op<Channel>{&channel}, token, channel);
}
} // aedis::detail

#include <boost/asio/unyield.hpp>
#endif // AEDIS_NET_HPP
