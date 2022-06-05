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

namespace aedis {
namespace resp3 {
namespace detail {

#include <boost/asio/yield.hpp>

template <
   class AsyncStream,
   class Command,
   class Adapter,
   class DynamicBuffer
   >
struct exec_op {
   AsyncStream* socket;
   request<Command> const* req;
   Adapter adapter;
   DynamicBuffer dbuf;
   boost::asio::coroutine coro;

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , std::size_t n = 0)
   {
      reenter (coro)
      {
         yield
         boost::asio::async_write(
            *socket,
            boost::asio::buffer(req->payload()),
            std::move(self));

         if (ec) {
            self.complete(ec, 0);
            return;
         }

         yield resp3::async_read(*socket, dbuf, adapter, std::move(self));
         self.complete(ec, n);
      }
   }
};

#include <boost/asio/unyield.hpp>

} // detail

template <
   class AsyncStream,
   class Command,
   class Adapter,
   class DynamicBuffer,
   class CompletionToken = boost::asio::default_completion_token_t<typename AsyncStream::executor_type>
   >
auto async_exec(
   AsyncStream& socket,
   request<Command> const& req,
   Adapter adapter,
   DynamicBuffer dbuf,
   CompletionToken token = CompletionToken{})
{
   return boost::asio::async_compose
      < CompletionToken
      , void(boost::system::error_code, std::size_t)
      >(detail::exec_op<AsyncStream, Command, Adapter, DynamicBuffer>
         {&socket, &req, adapter, dbuf}, token, socket);
}

namespace detail {

#include <boost/asio/yield.hpp>

template <
   class AsyncStream,
   class Command,
   class Adapter,
   class DynamicBuffer
   >
struct exec_with_timeout_op {
   AsyncStream* socket;
   boost::asio::steady_timer* timer;
   request<Command> const* req;
   Adapter adapter;
   DynamicBuffer dbuf;
   boost::asio::coroutine coro;

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
            [this](auto token) { return resp3::async_exec(*socket, *req, adapter, dbuf, token);},
            [this](auto token) { return timer->async_wait(token);}
         ).async_wait(
            boost::asio::experimental::wait_for_one(),
            std::move(self));

         switch (order[0]) {
            case 0:
            {
               if (ec1) {
                  self.complete(ec1, 0);
                  return;
               }
            } break;

            case 1:
            {
               if (!ec2) {
                  self.complete(aedis::error::idle_timeout, 0);
                  return;
               }
            } break;

            default: BOOST_ASSERT(false);
         }

         self.complete({}, n);
      }
   }
};

#include <boost/asio/unyield.hpp>

} // detail

template <
   class AsyncStream,
   class Command,
   class Adapter,
   class DynamicBuffer,
   class CompletionToken = boost::asio::default_completion_token_t<typename AsyncStream::executor_type>
   >
auto async_exec(
   AsyncStream& socket,
   boost::asio::steady_timer& timer,
   request<Command> const& req,
   Adapter adapter,
   DynamicBuffer dbuf,
   CompletionToken token = CompletionToken{})
{
   return boost::asio::async_compose
      < CompletionToken
      , void(boost::system::error_code, std::size_t)
      >(detail::exec_with_timeout_op<AsyncStream, Command, Adapter, DynamicBuffer>
         {&socket, &timer, &req, adapter, dbuf}, token, socket, timer);
}

} // resp3
} // aedis

#endif // AEDIS_RESP3_EXEC_HPP
