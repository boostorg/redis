/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>
#include <boost/redis/pubsub_response.hpp>
#include <boost/redis/resp3/node.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/consign.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/describe/class.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/value_to.hpp>

#include <iostream>
#include <vector>

#if defined(BOOST_ASIO_HAS_CO_AWAIT)

namespace asio = boost::asio;
using namespace std::chrono_literals;
using boost::redis::request;
using boost::redis::generic_flat_response;
using boost::redis::config;
using boost::system::error_code;
using boost::redis::connection;
using boost::redis::pubsub_message;
using asio::signal_set;

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

// Struct that will be stored in Redis using json serialization.
struct user {
   std::string name;
   std::string age;
   std::string country;
};

// The type must be described for serialization to work.
BOOST_DESCRIBE_STRUCT(user, (), (name, age, country))

void boost_redis_from_bulk(
   user& u,
   boost::redis::resp3::node_view const& node,
   boost::system::error_code&)
{
   u = boost::json::value_to<user>(boost::json::parse(node.value));
}

// Receives server pushes.
auto receiver(std::shared_ptr<connection> conn) -> asio::awaitable<void>
{
   request req;
   req.push("SUBSCRIBE", "channel");

   std::vector<pubsub_message<user>> resp;
   conn->set_receive_response(resp);

   // Loop while reconnection is enabled
   while (conn->will_reconnect()) {
      // Reconnect to the channels.
      co_await conn->async_exec(req);

      // Loop to read Redis push messages.
      for (error_code ec;;) {
         // Wait for pushes
         co_await conn->async_receive2(asio::redirect_error(ec));
         if (ec)
            break;  // Connection lost, break so we can reconnect to channels.

         // The response must be consumed without suspending the
         // coroutine i.e. without the use of async operations.
         for (auto const& elem : resp)
            std::cout << elem.channel << ": age=" << elem.payload.age
                      << ", name=" << elem.payload.name << "\n";

         std::cout << std::endl;

         resp.clear();
      }
   }
}

auto co_main(config cfg) -> asio::awaitable<void>
{
   auto ex = co_await asio::this_coro::executor;
   auto conn = std::make_shared<connection>(ex);
   asio::co_spawn(ex, receiver(conn), asio::detached);
   conn->async_run(cfg, asio::consign(asio::detached, conn));

   signal_set sig_set(ex, SIGINT, SIGTERM);
   co_await sig_set.async_wait();

   conn->cancel();
}

#endif  // defined(BOOST_ASIO_HAS_CO_AWAIT)
