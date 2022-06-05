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

namespace aedis {
namespace detail {

#include <boost/asio/yield.hpp>

template <
   class Protocol,
   class Executor,
   class EndpointSequence
   >
struct connect_op {
   boost::asio::basic_socket<Protocol, Executor>* socket;
   boost::asio::steady_timer* timer;
   EndpointSequence* endpoints;
   boost::asio::coroutine coro;

   template <class Self>
   void operator()( Self& self
                  , std::array<std::size_t, 2> order = {}
                  , boost::system::error_code ec1 = {}
                  , typename Protocol::endpoint const& ep = {}
                  , boost::system::error_code ec2 = {})
   {
      reenter (coro)
      {
         yield
         boost::asio::experimental::make_parallel_group(
            [this](auto token)
            {
               auto f = [](boost::system::error_code const&, typename Protocol::endpoint const&) { return true; };
               return boost::asio::async_connect(*socket, *endpoints, f, token);
            },
            [this](auto token) { return timer->async_wait(token);}
         ).async_wait(
            boost::asio::experimental::wait_for_one(),
            std::move(self));

         switch (order[0]) {
            case 0:
            {
               if (ec1) {
                  self.complete(ec1, ep);
                  return;
               }
            } break;

            case 1:
            {
               if (!ec2) {
                  self.complete(error::connect_timeout, ep);
                  return;
               }
            } break;

            default: BOOST_ASSERT(false);
         }

         self.complete({}, ep);
      }
   }
};

struct resolve_op {
   boost::asio::ip::tcp::resolver* resv;
   boost::asio::steady_timer* timer;
   boost::string_view host;
   boost::string_view port;
   boost::asio::coroutine coro;

   template <class Self>
   void operator()( Self& self
                  , std::array<std::size_t, 2> order = {}
                  , boost::system::error_code ec1 = {}
                  , boost::asio::ip::tcp::resolver::results_type res = {}
                  , boost::system::error_code ec2 = {})
   {
      reenter (coro)
      {
         yield
         boost::asio::experimental::make_parallel_group(
            [this](auto token) { return resv->async_resolve(host.data(), port.data(), token);},
            [this](auto token) { return timer->async_wait(token);}
         ).async_wait(
            boost::asio::experimental::wait_for_one(),
            std::move(self));

         switch (order[0]) {
            case 0:
            {
               if (ec1) {
                  self.complete(ec1, {});
                  return;
               }
            } break;

            case 1:
            {
               if (!ec2) {
                  self.complete(error::resolve_timeout, {});
                  return;
               }
            } break;

            default: BOOST_ASSERT(false);
         }

         self.complete({}, res);
      }
   }
};

#include <boost/asio/unyield.hpp>

template <
   class Protocol,
   class Executor,
   class EndpointSequence,
   class CompletionToken = boost::asio::default_completion_token_t<Executor>
   >
auto async_connect(
      boost::asio::basic_socket<Protocol, Executor>& socket,
      boost::asio::steady_timer& timer,
      EndpointSequence ep,
      CompletionToken&& token = boost::asio::default_completion_token_t<Executor>{})
{
   return boost::asio::async_compose
      < CompletionToken
      , void(boost::system::error_code, typename Protocol::endpoint const&)
      >(connect_op<Protocol, Executor, EndpointSequence>
            {&socket, &timer, &ep}, token, socket, timer);
}

template <
   class CompletionToken =
      boost::asio::default_completion_token_t<boost::asio::ip::tcp::resolver::executor_type>
   >
auto async_resolve(
      boost::asio::ip::tcp::resolver& resv,
      boost::asio::steady_timer& timer,
      boost::string_view host,
      boost::string_view port,
      CompletionToken&& token = CompletionToken{})
{
   return boost::asio::async_compose
      < CompletionToken
      , void(boost::system::error_code, boost::asio::ip::tcp::resolver::results_type)
      >(resolve_op{&resv, &timer, host, port}, token, resv, timer);
}

} // detail
} // aedis

#endif // AEDIS_NET_HPP
