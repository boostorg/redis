/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <tuple>
#include <string>
#include <thread>
#include <boost/asio.hpp>
#include <aedis.hpp>
#include "print.hpp"

// Include this in no more than one .cpp file.
#include <aedis/src.hpp>

namespace net = boost::asio;
using aedis::adapt;
using aedis::resp3::node;
using aedis::resp3::request;
using aedis::endpoint;
using connection = aedis::sync<aedis::connection<>>;

// See subscriber.cpp for more info about how to run this example.

// Subscribe again everytime there is a disconnection.
void reconnect(connection& conn)
{
   request req;
   req.push("SUBSCRIBE", "channel");

   endpoint ep{"127.0.0.1", "6379"};
   for (;;) {
      boost::system::error_code ec;
      conn.run(ep, req, adapt(), ec);
      conn.reset_stream();
      std::cout << ec.message() << std::endl;
      std::this_thread::sleep_for(std::chrono::seconds{1});
   }
}

int main()
{
   try {
      net::io_context ioc{1};
      auto work = net::make_work_guard(ioc);

      connection conn{work.get_executor()};

      std::thread t1{[&]() { ioc.run(); }};
      std::thread t2{[&]() { reconnect(conn); }};

      for (std::vector<node<std::string>> resp;;) {
         conn.receive_push(adapt(resp));
         print_push(resp);
         resp.clear();
      }

      t1.join();
      t2.join();

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
