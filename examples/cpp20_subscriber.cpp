/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>
#include <boost/redis/logger.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/consign.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/signal_set.hpp>
#include <iostream>

#if defined(BOOST_ASIO_HAS_CO_AWAIT)

namespace net = boost::asio;
using namespace std::chrono_literals;
using boost::redis::request;
using boost::redis::generic_response;
using boost::redis::logger;
using boost::redis::config;
using boost::system::error_code;
using connection = net::deferred_t::as_default_on_t<boost::redis::connection>;
using signal_set = net::deferred_t::as_default_on_t<net::signal_set>;

/* This example will subscribe and read pushes indefinitely.
 *
 * To test send messages with redis-cli
 *
 *    $ redis-cli -3
 *    127.0.0.1:6379> PUBLISH channel some-message
 *    (integer) 3
 *    127.0.0.1:6379>
 *
 * To test reconnection try, for example, to close all clients currently
 * connected to the Redis instance
 *
 * $ redis-cli
 * > CLIENT kill TYPE pubsub
 */

// Receives server pushes.
auto
receiver(std::shared_ptr<connection> conn) -> net::awaitable<void>
{
   request req;
   req.push("SUBSCRIBE", "channel");

   while (conn->will_reconnect()) {

      // Reconnect to channels.
      co_await conn->async_exec(req);

      // Loop reading Redis pushs messages.
      for (generic_response resp;;) {
         error_code ec;
         co_await conn->async_receive(resp, net::redirect_error(net::use_awaitable, ec));
         if (ec)
            break; // Connection lost, break so we can reconnect to channels.
         std::cout
            << resp.value().at(1).value
            << " " << resp.value().at(2).value
            << " " << resp.value().at(3).value
            << std::endl;
         resp.value().clear();
      }
   }
}

auto co_main(config cfg) -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;
   auto conn = std::make_shared<connection>(ex);
   net::co_spawn(ex, receiver(conn), net::detached);
   conn->async_run(cfg, {}, net::consign(net::detached, conn));

   signal_set sig_set(ex, SIGINT, SIGTERM);
   co_await sig_set.async_wait();

   conn->cancel();
}

#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
