/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/experimental/connector.hpp>
#include <boost/redis/logger.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/consign.hpp>
#include <iostream>

#if defined(BOOST_ASIO_HAS_CO_AWAIT)

namespace net = boost::asio;
namespace redis = boost::redis;
using redis::generic_response;
using redis::address;
using redis::logger;
using redis::experimental::async_connect;
using redis::experimental::connect_config;
using connection = net::use_awaitable_t<>::as_default_on_t<redis::connection>;

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
auto receiver(std::shared_ptr<connection> conn) -> net::awaitable<void>
{
   for (generic_response resp;;) {
      co_await conn->async_receive(resp);
      std::cout
         << resp.value().at(1).value
         << " " << resp.value().at(2).value
         << " " << resp.value().at(3).value
         << std::endl;
      resp.value().clear();
   }
}

auto co_main(address const& addr) -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;
   auto conn = std::make_shared<connection>(ex);
   connect_config cfg;
   cfg.addr = addr;
   net::co_spawn(ex, receiver(conn), net::detached);
   redis::experimental::async_connect(*conn, cfg, logger{}, net::consign(net::detached, conn));
}

#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
