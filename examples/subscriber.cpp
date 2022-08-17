/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <string>
#include <vector>
#include <iostream>
#include <tuple>
#include <boost/asio.hpp>
#include <aedis.hpp>
#include "print.hpp"

// Include this in no more than one .cpp file.
#include <aedis/src.hpp>

namespace net = boost::asio;
using aedis::adapt;
using aedis::resp3::request;
using aedis::resp3::node;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::socket>;
using connection = aedis::connection<tcp_socket>;

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

// Receives pushes.
net::awaitable<void> push_receiver(std::shared_ptr<connection> db)
{
   for (std::vector<node<std::string>> resp;;) {
      co_await db->async_receive_push(adapt(resp));
      print_push(resp);
      resp.clear();
   }
}

// Receives events.
net::awaitable<void> event_receiver(std::shared_ptr<connection> db)
{
   request req;
   req.push("SUBSCRIBE", "channel");

   for (;;) {
      auto ev = co_await db->async_receive_event();
      if (ev == connection::event::hello)
         co_await db->async_exec(req);
   }
}

int main()
{
   try {
      net::io_context ioc;
      auto db = std::make_shared<connection>(ioc);

      db->get_config().enable_events = true;
      db->get_config().enable_reconnect = true;

      net::co_spawn(ioc, push_receiver(db), net::detached);
      net::co_spawn(ioc, event_receiver(db), net::detached);
      db->async_run(net::detached);

      net::signal_set signals(ioc, SIGINT, SIGTERM);
      signals.async_wait([&](auto, auto){ ioc.stop(); });

      ioc.run();
   } catch (std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
   }
}
