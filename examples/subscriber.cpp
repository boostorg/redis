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
#include <boost/asio/experimental/as_tuple.hpp>
#include <aedis.hpp>
#include "print.hpp"

// Include this in no more than one .cpp file.
#include <aedis/src.hpp>


namespace net = boost::asio;
using aedis::adapt;
using aedis::resp3::request;
using node_type = aedis::resp3::node<std::string>;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::socket>;
using connection = aedis::connection<tcp_socket>;
using net::experimental::as_tuple;

/* This example will subscribe and read pushes indefinitely.
 *
 * To test send messages with redis-cli
 *
 *    $ redis-cli -3
 *    127.0.0.1:6379> PUBLISH channel1 some-message
 *    (integer) 3
 *    127.0.0.1:6379>
 *
 * To test reconnection try, for example, to close all clients currently
 * connected to the Redis instance
 *
 * $ redis-cli
 * > CLIENT kill TYPE pubsub
 */

net::awaitable<void> receiver(std::shared_ptr<connection> db)
{
   request req;
   req.push("SUBSCRIBE", "channel");

   for (std::vector<node_type> resp;;) {
      auto [ec, ev] = co_await db->async_receive_event(aedis::adapt(resp), as_tuple(net::use_awaitable));

      std::cout << "Event: " << aedis::to_string<tcp_socket>(ev) << std::endl;

      switch (ev) {
         case connection::event::push:
         print_push(resp);
         resp.clear();
         break;

         case connection::event::hello:
         // Subscribes to the channels when a new connection is
         // stablished.
         co_await db->async_exec(req);
         break;

         default:;
      }
   }
}

int main()
{
   try {
      net::io_context ioc;
      auto db = std::make_shared<connection>(ioc);

      db->get_config().enable_events = true;
      db->get_config().enable_reconnect = true;

      net::co_spawn(ioc, receiver(db), net::detached);
      db->async_run(net::detached);
      net::signal_set signals(ioc, SIGINT, SIGTERM);
      signals.async_wait([&](auto, auto){ ioc.stop(); });
      ioc.run();
   } catch (std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
   }
}
