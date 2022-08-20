/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <tuple>
#include <string>
#include <boost/asio.hpp>
#include <aedis.hpp>
#include "print.hpp"

// Include this in no more than one .cpp file.
#include <aedis/src.hpp>

namespace net = boost::asio;
using aedis::adapt;
using aedis::resp3::node;
using aedis::resp3::request;
using connection = aedis::sync<aedis::connection<>>;
using event = connection::event;

// See subscriber.cpp for more info about how to run this example.

// Subscribe again everytime there is a disconnection.
void event_receiver(connection& conn)
{
   request req;
   req.push("SUBSCRIBE", "channel");

   for (;;) {
      auto ev = conn.receive_event();
      if (ev == connection::event::hello)
         conn.exec(req);
   }
}

int main()
{
   try {
      net::io_context ioc{1};
      auto work = net::make_work_guard(ioc);

      connection::config cfg;
      cfg.enable_events = true;
      cfg.enable_reconnect = true;
      connection conn{work.get_executor(), cfg};

      std::thread t1{[&]() { ioc.run(); }};
      std::thread t2{[&]() { boost::system::error_code ec; conn.run(ec); }};
      std::thread t3{[&]() { event_receiver(conn); }};

      for (std::vector<node<std::string>> resp;;) {
         conn.receive_push(adapt(resp));
         print_push(resp);
         resp.clear();
      }

      t1.join();
      t2.join();
      t3.join();

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
