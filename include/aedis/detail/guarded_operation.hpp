/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_DETAIL_GUARDED_OPERATION_HPP
#define AEDIS_DETAIL_GUARDED_OPERATION_HPP

#include <boost/asio/experimental/channel.hpp>

namespace aedis::detail {

template <class Executor>
struct send_receive_op {
   using channel_type = boost::asio::experimental::channel<Executor, void(boost::system::error_code, std::size_t)>;
   channel_type* channel;
   boost::asio::coroutine coro{};

   template <class Self>
   void operator()(Self& self, boost::system::error_code ec = {})
   {
      BOOST_ASIO_CORO_REENTER (coro)
      {
         BOOST_ASIO_CORO_YIELD
         channel->async_send(boost::system::error_code{}, 0, std::move(self));
         AEDIS_CHECK_OP0(;);

         BOOST_ASIO_CORO_YIELD
         channel->async_send(boost::system::error_code{}, 0, std::move(self));
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
      BOOST_ASIO_CORO_REENTER (coro)
      {
         BOOST_ASIO_CORO_YIELD
         channel->async_receive(std::move(self));
         AEDIS_CHECK_OP1(;);

         BOOST_ASIO_CORO_YIELD
         std::move(op)(std::move(self));
         AEDIS_CHECK_OP1(channel->cancel(););

         res = n;

         BOOST_ASIO_CORO_YIELD
         channel->async_receive(std::move(self));
         AEDIS_CHECK_OP1(;);

         self.complete({}, res);
         return;
      }
   }
};

template <class Executor>
class guarded_operation {
public:
   using executor_type = Executor;
   guarded_operation(executor_type ex) : channel_{ex} {}

   template <class CompletionToken>
   auto async_run(CompletionToken&& token)
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(send_receive_op<executor_type>{&channel_}, token, channel_);
   }

   template <class Op, class CompletionToken>
   auto async_wait(Op&& op, CompletionToken token)
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

#endif // AEDIS_DETAIL_GUARDED_OPERATION_HPP
