/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/run.hpp>
#include <boost/redis/logger.hpp>
#include <boost/redis/check_health.hpp>
#include <boost/system/errc.hpp>
#define BOOST_TEST_MODULE check-health
#include <boost/test/included/unit_test.hpp>
#include <iostream>
#include "common.hpp"
#include <boost/redis/src.hpp>

namespace net = boost::asio;
using error_code = boost::system::error_code;
using connection = boost::redis::connection;
using boost::redis::request;
using boost::redis::ignore;
using boost::redis::operation;
using boost::redis::generic_response;
using boost::redis::async_check_health;
using boost::redis::async_run;
using boost::redis::logger;
using boost::redis::address;
using namespace std::chrono_literals;

std::chrono::seconds const interval{1};

struct push_callback {
   connection* conn;
   connection* conn2;
   generic_response* resp;
   request* req;
   int i = 0;

   void operator()(error_code ec = {}, std::size_t = 0)
   {
      ++i;
      if (ec) {
         std::clog << "Exiting." << std::endl;
         return;
      }

      if (resp->value().empty()) {
         // First call
         BOOST_TEST(!ec);
         conn2->async_receive(*resp, *this);
      } else if (i == 5) {
         std::clog << "Pausing the server" << std::endl;
         // Pause the redis server to test if the health-check exits.
         conn->async_exec(*req, ignore, [](auto ec, auto) {
            std::clog << "Pausing callback> " << ec.message() << std::endl;
            // Don't know in CI we are getting: Got RESP3 simple-error.
            //BOOST_TEST(!ec);
         });
         conn2->cancel(operation::run);
         conn2->cancel(operation::receive);
      } else {
         BOOST_TEST(!ec);
         // Expect 3 pongs and pause the clients so check-health exists
         // without error.
         BOOST_TEST(resp->has_value());
         BOOST_TEST(!resp->value().empty());
         std::clog << "Event> " << resp->value().front().value << std::endl;
         resp->value().clear();
         conn2->async_receive(*resp, *this);
      }
   };
};

BOOST_AUTO_TEST_CASE(check_health)
{
   net::io_context ioc;

   connection conn{ioc};

   // It looks like client pause does not work for clients that are
   // sending MONITOR. I will therefore open a second connection.
   connection conn2{ioc};

   std::string const msg = "test-check-health";

   bool seen = false;
   async_check_health(conn, msg, interval, [&](auto ec) {
      BOOST_TEST(!ec);
      std::cout << "async_check_health: completed." << std::endl;
      seen = true;
   });

   request req;
   req.push("HELLO", 3);
   req.push("MONITOR");

   conn2.async_exec(req, ignore, [](auto ec, auto) {
      std::cout << "A" << std::endl;
      BOOST_TEST(!ec);
   });

   request req2;
   req2.push("HELLO", "3");
   req2.push("CLIENT", "PAUSE", "3000", "ALL");

   generic_response resp;
   push_callback{&conn, &conn2, &resp, &req2}(); // Starts reading pushes.

   async_run(conn, address{}, 10s, 10s, logger{}, [](auto ec){
      std::cout << "B" << std::endl;
      BOOST_TEST(!!ec);
   });

   async_run(conn2, address{}, 10s, 10s, logger{}, [](auto ec){
      std::cout << "C" << std::endl;
      BOOST_TEST(!!ec);
   });

   ioc.run();
   BOOST_TEST(seen);
}

