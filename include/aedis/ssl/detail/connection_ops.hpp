/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_SSL_CONNECTION_OPS_HPP
#define AEDIS_SSL_CONNECTION_OPS_HPP

#include <array>

#include <boost/assert.hpp>
#include <boost/system.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/yield.hpp>

namespace aedis::ssl::detail
{

template <class Stream>
struct handshake_op {
   Stream* stream;
   aedis::detail::conn_timer_t<typename Stream::executor_type>* timer;
   boost::asio::coroutine coro{};

   template <class Self>
   void operator()( Self& self
                  , std::array<std::size_t, 2> order = {}
                  , boost::system::error_code ec1 = {}
                  , boost::system::error_code ec2 = {})
   {
      reenter (coro)
      {
         yield
         boost::asio::experimental::make_parallel_group(
            [this](auto token)
            {
               return stream->async_handshake(boost::asio::ssl::stream_base::client, token);
            },
            [this](auto token) { return timer->async_wait(token);}
         ).async_wait(
            boost::asio::experimental::wait_for_one(),
            std::move(self));

         if (is_cancelled(self)) {
            self.complete(boost::asio::error::operation_aborted);
            return;
         }

         switch (order[0]) {
            case 0: self.complete(ec1); return;
            case 1:
            {
               BOOST_ASSERT_MSG(!ec2, "handshake_op: Incompatible state.");
               self.complete(error::ssl_handshake_timeout);
               return;
            }

            default: BOOST_ASSERT(false);
         }
      }
   }
};

template <
   class Stream,
   class CompletionToken
   >
auto async_handshake(
      Stream& stream,
      aedis::detail::conn_timer_t<typename Stream::executor_type>& timer,
      CompletionToken&& token)
{
   return boost::asio::async_compose
      < CompletionToken
      , void(boost::system::error_code)
      >(handshake_op<Stream>{&stream, &timer}, token, stream, timer);
}

template <class Conn, class Timer>
struct ssl_connect_with_timeout_op {
   Conn* conn = nullptr;
   boost::asio::ip::tcp::resolver::results_type const* endpoints = nullptr;
   typename Conn::timeouts ts;
   Timer* timer = nullptr;
   boost::asio::coroutine coro{};

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , boost::asio::ip::tcp::endpoint const& = {})
   {
      reenter (coro)
      {
         timer->expires_after(ts.connect_timeout);
         yield
         aedis::detail::async_connect(
            conn->lowest_layer(), *timer, *endpoints, std::move(self));
         AEDIS_CHECK_OP0();

         timer->expires_after(ts.handshake_timeout);
         yield
         async_handshake(conn->next_layer(), *timer, std::move(self));
         AEDIS_CHECK_OP0();
         self.complete({});
      }
   }
};

} // aedis::ssl::detail
 
#include <boost/asio/unyield.hpp>
#endif // AEDIS_SSL_CONNECTION_OPS_HPP
