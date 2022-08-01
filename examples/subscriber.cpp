/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <string>
#include <vector>
#include <iostream>
#include <boost/asio.hpp>
#include <boost/asio/experimental/as_tuple.hpp>
#include <aedis.hpp>

// Include this in no more than one .cpp file.
#include <aedis/src.hpp>

namespace net = boost::asio;
using aedis::adapt;
using aedis::resp3::request;
using aedis::resp3::node;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::socket>;
using connection = aedis::connection<tcp_socket>;
using net::experimental::as_tuple;

/* In this example we send a subscription to a channel and start
 * reading server side messages indefinitely.
 *
 * After starting the example you can test it by sending messages with
 * redis-cli like this
 *
 *    $ redis-cli -3
 *    127.0.0.1:6379> PUBLISH channel1 some-message
 *    (integer) 3
 *    127.0.0.1:6379>
 *
 * The messages will then appear on the terminal you are running the
 * example.
 *
 * To test reconnection try for example to close all clients currently
 * connected to the Redis instance
 *
 * $ redis-cli
 * > CLIENT kill TYPE pubsub
 */

net::awaitable<void> reader(std::shared_ptr<connection> db)
{
   try {
      for (std::vector<node<std::string>> resp;;) {
         co_await db->async_receive(adapt(resp));

         std::cout
            << "Event: "   << resp.at(1).value << "\n"
            << "Channel: " << resp.at(2).value << "\n"
            << "Message: " << resp.at(3).value << "\n"
            << std::endl;

         resp.clear();
      }
   } catch (std::exception const& e) {
      std::cerr << "Reader: " << e.what() << std::endl;
   }
}

net::awaitable<void> reconnect(std::shared_ptr<connection> db)
{
   auto ex = co_await net::this_coro::executor;

   request req;
   req.push("SUBSCRIBE", "channel");

   net::steady_timer timer{ex};

   for (;;) {
      co_await db->async_run(req, adapt(), as_tuple(net::use_awaitable));

      // Waits one second and tries again.
      timer.expires_after(std::chrono::seconds{1});
      co_await timer.async_wait(net::use_awaitable);
   }
}

int main()
{
   try {
      net::io_context ioc;
      auto db = std::make_shared<connection>(ioc);
      net::co_spawn(ioc, reader(db), net::detached);
      net::co_spawn(ioc, reconnect(db), net::detached);
      net::signal_set signals(ioc, SIGINT, SIGTERM);
      signals.async_wait([&](auto, auto){ ioc.stop(); });
      ioc.run();
   } catch (std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
   }
}
