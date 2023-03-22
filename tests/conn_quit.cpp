/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/run.hpp>
#include <boost/redis/logger.hpp>
#include <boost/system/errc.hpp>
#define BOOST_TEST_MODULE conn-quit
#include <boost/test/included/unit_test.hpp>
#include "common.hpp"
#include <iostream>
#include <boost/redis/src.hpp>

namespace net = boost::asio;

using connection = boost::redis::connection;
using error_code = boost::system::error_code;
using operation = boost::redis::operation;
using boost::redis::request;
using boost::redis::response;
using boost::redis::ignore;
using boost::redis::async_run;
using boost::redis::logger;
using boost::redis::address;
using namespace std::chrono_literals;

BOOST_AUTO_TEST_CASE(test_quit1)
{
   request req;
   req.get_config().cancel_on_connection_lost = false;
   req.push("HELLO", 3);
   req.push("QUIT");

   net::io_context ioc;
   connection conn{ioc};

   conn.async_exec(req, ignore, [](auto ec, auto) {
      BOOST_TEST(!ec);
   });

   async_run(conn, address{}, 10s, 10s, logger{}, [&](auto ec){
      BOOST_TEST(!ec);
   });

   ioc.run();
}

// Test if quit causes async_run to exit.
BOOST_AUTO_TEST_CASE(test_quit2)
{
   net::io_context ioc;

   connection conn{ioc};

   request req1;
   req1.get_config().cancel_on_connection_lost = false;
   req1.push("PING");

   request req2;
   req2.get_config().cancel_on_connection_lost = false;
   req2.push("QUIT");

   request req3;
   // Should cause the request to fail since this request will be sent
   // after quit.
   req3.get_config().cancel_if_not_connected = true;
   req3.push("PING");

   auto c3 = [](auto ec, auto)
   {
      std::cout << "3--> " << ec.message() << std::endl;
      BOOST_CHECK_EQUAL(ec, boost::system::errc::errc_t::operation_canceled);
   };

   auto c2 = [&](auto ec, auto)
   {
      std::cout << "2--> " << ec.message() << std::endl;
      BOOST_TEST(!ec);
      conn.async_exec(req3, ignore, c3);
   };

   auto c1 = [&](auto ec, auto)
   {
      std::cout << "1--> " << ec.message() << std::endl;
      BOOST_TEST(!ec);

      conn.async_exec(req2, ignore, c2);
   };

   conn.async_exec(req1, ignore, c1);

   async_run(conn, address{}, 10s, 10s, logger{}, [&](auto ec){
      BOOST_TEST(!ec);
   });

   ioc.run();
}

