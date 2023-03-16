/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/run.hpp>
#define BOOST_TEST_MODULE run
#include <boost/test/included/unit_test.hpp>
#include <iostream>
#include <boost/redis/src.hpp>

namespace net = boost::asio;

using connection = boost::redis::connection;
using boost::redis::async_run;
using boost::redis::address;
using boost::system::error_code;
using namespace std::chrono_literals;

bool is_host_not_found(error_code ec)
{
   if (ec == net::error::netdb_errors::host_not_found) return true;
   if (ec == net::error::netdb_errors::host_not_found_try_again) return true;
   return false;
}

BOOST_AUTO_TEST_CASE(resolve_bad_host)
{
   net::io_context ioc;

   connection conn{ioc};
   async_run(conn, address{{"Atibaia"}, {"6379"}}, 1000s, 1000s, [](auto ec){
      BOOST_TEST(is_host_not_found(ec));
   });

   ioc.run();
}

BOOST_AUTO_TEST_CASE(resolve_with_timeout)
{
   net::io_context ioc;

   connection conn{ioc};
   async_run(conn, address{{"Atibaia"}, {"6379"}}, 1ms, 1ms, [](auto ec){
      BOOST_CHECK_EQUAL(ec, boost::redis::error::resolve_timeout);
   });

   ioc.run();
}

BOOST_AUTO_TEST_CASE(connect_bad_port)
{
   net::io_context ioc;

   connection conn{ioc};
   async_run(conn, address{{"127.0.0.1"}, {"1"}}, 1000s, 10s, [](auto ec){
      BOOST_CHECK_EQUAL(ec, net::error::basic_errors::connection_refused);
   });

   ioc.run();
}

BOOST_AUTO_TEST_CASE(connect_with_timeout)
{
   net::io_context ioc;

   connection conn{ioc};
   async_run(conn, address{{"example.com"}, {"1"}}, 10s, 1ms, [](auto ec){
      BOOST_CHECK_EQUAL(ec, boost::redis::error::connect_timeout);
   });

   ioc.run();
}

