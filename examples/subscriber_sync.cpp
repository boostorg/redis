/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <tuple>
#include <string>
#include <boost/asio.hpp>
#include <aedis.hpp>
#include <aedis/experimental/sync.hpp>
#include "print.hpp"

// Include this in no more than one .cpp file.
#include <aedis/src.hpp>

namespace net = boost::asio;
using aedis::resp3::request;
using aedis::experimental::exec;
using aedis::experimental::receive_event;
using aedis::experimental::receive_push;
using connection = aedis::connection<>;
using aedis::resp3::node;
using aedis::adapt;
using event = connection::event;

// See subscriber.cpp for more info about how to run this example.

void push_receiver(connection& conn)
{
   for (std::vector<node<std::string>> resp;;) {
      receive_push(conn, adapt(resp));
      print_push(resp);
      resp.clear();
   }
}

void event_receiver(connection& conn)
{
   request req;
   req.push("SUBSCRIBE", "channel");

   for (;;) {
      switch (receive_event(conn)) {
         case connection::event::hello:
         exec(conn, req);
         break;

         default:;
      }
   }
}

int main()
{
   try {
      net::io_context ioc{1};
      connection conn{ioc};

      conn.get_config().enable_events = true;
      conn.get_config().enable_reconnect = true;

      std::thread push_thread{[&]() { push_receiver(conn); }};
      std::thread event_thread{[&]() { event_receiver(conn); }};

      conn.async_run(net::detached);
      ioc.run();

      event_thread.join();
      push_thread.join();

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
