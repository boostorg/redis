/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_RESP3_EXEC_HPP
#define AEDIS_RESP3_EXEC_HPP

#include <boost/assert.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/experimental/parallel_group.hpp>

#include <aedis/error.hpp>
#include <aedis/resp3/read.hpp>
#include <aedis/resp3/request.hpp>

namespace aedis::resp3::detail {

#include <boost/asio/yield.hpp>

template <
   class AsyncStream,
   class Adapter,
   class DynamicBuffer
   >
struct exec_op {
   AsyncStream* socket = nullptr;
   request const* req = nullptr;
   Adapter adapter;
   DynamicBuffer dbuf{};
   std::size_t n_cmds = 0;
   std::size_t size = 0;
   boost::asio::coroutine coro{};

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , std::size_t n = 0)
   {
      reenter (coro) for (;;)
      {
         if (req) {
            yield
            boost::asio::async_write(
               *socket,
               boost::asio::buffer(req->payload()),
               std::move(self));

            if (ec || n_cmds == 0) {
               self.complete(ec, n);
               return;
            }

            req = nullptr;
         }

         yield resp3::async_read(*socket, dbuf, adapter, std::move(self));
         if (ec) {
            self.complete(ec, 0);
            return;
         }

         size += n;
         if (--n_cmds == 0) {
            self.complete(ec, size);
            return;
         }
      }
   }
};

template <
   class AsyncStream,
   class Adapter,
   class DynamicBuffer,
   class CompletionToken = boost::asio::default_completion_token_t<typename AsyncStream::executor_type>
   >
auto async_exec(
   AsyncStream& socket,
   request const& req,
   Adapter adapter,
   DynamicBuffer dbuf,
   CompletionToken token = CompletionToken{})
{
   return boost::asio::async_compose
      < CompletionToken
      , void(boost::system::error_code, std::size_t)
      >(detail::exec_op<AsyncStream, Adapter, DynamicBuffer>
         {&socket, &req, adapter, dbuf, req.size()}, token, socket);
}

template <
   class AsyncStream,
   class Timer,
   class Adapter,
   class DynamicBuffer
   >
struct exec_with_timeout_op {
   AsyncStream* socket = nullptr;
   Timer* timer = nullptr;
   request const* req = nullptr;
   Adapter adapter;
   DynamicBuffer dbuf{};
   boost::asio::coroutine coro{};

   template <class Self>
   void operator()( Self& self
                  , std::array<std::size_t, 2> order = {}
                  , boost::system::error_code ec1 = {}
                  , std::size_t n = 0
                  , boost::system::error_code ec2 = {})
   {
      reenter (coro)
      {
         yield
         boost::asio::experimental::make_parallel_group(
            [this](auto token) { return detail::async_exec(*socket, *req, adapter, dbuf, token);},
            [this](auto token) { return timer->async_wait(token);}
         ).async_wait(
            boost::asio::experimental::wait_for_one(),
            std::move(self));

         switch (order[0]) {
            case 0: self.complete(ec1, n); break;
            case 1:
            {
               BOOST_ASSERT_MSG(!ec2, "exec_with_timeout_op: Unexpected completion order.");
               self.complete(aedis::error::exec_timeout, 0);

            } break;

            default: BOOST_ASSERT(false);
         }
      }
   }
};

#include <boost/asio/unyield.hpp>

template <
   class AsyncStream,
   class Timer,
   class Adapter,
   class DynamicBuffer,
   class CompletionToken = boost::asio::default_completion_token_t<typename AsyncStream::executor_type>
   >
auto async_exec(
   AsyncStream& socket,
   Timer& timer,
   request const& req,
   Adapter adapter,
   DynamicBuffer dbuf,
   CompletionToken token = CompletionToken{})
{
   return boost::asio::async_compose
      < CompletionToken
      , void(boost::system::error_code, std::size_t)
      >(detail::exec_with_timeout_op<AsyncStream, Timer, Adapter, DynamicBuffer>
         {&socket, &timer, &req, adapter, dbuf}, token, socket, timer);
}

} // aedis::resp3::detail

#endif // AEDIS_RESP3_EXEC_HPP
