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
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/experimental/parallel_group.hpp>

#include <boost/asio/yield.hpp>

namespace aedis::detail {

template <class Executor>
struct send_receive_op {
   using channel_type = boost::asio::experimental::channel<Executor, void(boost::system::error_code, std::size_t)>;
   channel_type* channel;
   boost::asio::coroutine coro{};

   template <class Self>
   void operator()(Self& self, boost::system::error_code ec = {})
   {
      reenter (coro)
      {
         yield channel->async_send(boost::system::error_code{}, 0, std::move(self));
         AEDIS_CHECK_OP0(;);

         yield channel->async_send(boost::system::error_code{}, 0, std::move(self));
         AEDIS_CHECK_OP0(;);

         self.complete({});
      }
   }
};

template <class Executor, class Op>
struct wait_op {
   using channel_type = boost::asio::experimental::channel<Executor, void(boost::system::error_code, std::size_t)>;
   channel_type* channel;
   Op op;
   std::size_t res = 0;
   boost::asio::coroutine coro{};

   template <class Self>
   void
   operator()( Self& self
             , boost::system::error_code ec = {}
             , std::size_t n = 0)
   {
      reenter (coro)
      {
         yield channel->async_receive(std::move(self));
         AEDIS_CHECK_OP1(;);

         yield std::move(op)(std::move(self));
         AEDIS_CHECK_OP1(channel->cancel(););

         res = n;

         yield channel->async_receive(std::move(self));
         AEDIS_CHECK_OP1(;);

         self.complete({}, res);
         return;
      }
   }
};

template <class Executor = boost::asio::any_io_executor>
class guarded_operation {
public:
   using executor_type = Executor;
   guarded_operation(executor_type ex) : channel_{ex} {}

   template <class CompletionToken = boost::asio::default_completion_token_t<executor_type>>
   auto async_run(CompletionToken&& token = CompletionToken{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(send_receive_op<executor_type>{&channel_}, token, channel_);
   }

   template <class Op, class CompletionToken = boost::asio::default_completion_token_t<executor_type>>
   auto async_wait(Op&& op, CompletionToken token = CompletionToken{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code, std::size_t)
         >(wait_op<executor_type, Op>{&channel_, std::move(op)}, token, channel_);
   }

   void cancel() {channel_.cancel();}

private:
   using channel_type = boost::asio::experimental::channel<executor_type, void(boost::system::error_code, std::size_t)>;

   template <class> friend struct send_receive_op;
   template <class, class> friend struct wait_op;

   channel_type channel_;
};

} // aedis::detail

#include <boost/asio/unyield.hpp>
#endif // AEDIS_NET_HPP
