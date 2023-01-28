/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <iostream>
#include <boost/asio.hpp>
#include <boost/system/errc.hpp>

#define BOOST_TEST_MODULE low level
#include <boost/test/included/unit_test.hpp>

#include <boost/redis.hpp>
#include <boost/redis/src.hpp>
#include "common.hpp"

namespace net = boost::asio;

using boost::redis::adapt;
using boost::redis::resp3::request;
using connection = boost::redis::connection;
using error_code = boost::system::error_code;
using operation = boost::redis::operation;

// Test if quit causes async_run to exit.
BOOST_AUTO_TEST_CASE(test_quit_no_coalesce)
{
   net::io_context ioc;

   auto const endpoints = resolve();
   connection conn{ioc};
   net::connect(conn.next_layer(), endpoints);

   request req1;
   req1.get_config().cancel_on_connection_lost = false;
   req1.get_config().coalesce = false;
   req1.push("PING");

   request req2;
   req2.get_config().cancel_on_connection_lost = false;
   req2.get_config().coalesce = false;
   req2.push("QUIT");

   conn.async_exec(req1, adapt(), [](auto ec, auto){
      BOOST_TEST(!ec);
   });
   conn.async_exec(req2, adapt(), [](auto ec, auto) {
      BOOST_TEST(!ec);
   });
   conn.async_exec(req1, adapt(), [](auto ec, auto){
      BOOST_CHECK_EQUAL(ec, boost::system::errc::errc_t::operation_canceled);
   });
   conn.async_exec(req1, adapt(), [](auto ec, auto){
         BOOST_CHECK_EQUAL(ec, boost::system::errc::errc_t::operation_canceled);
   });
   conn.async_exec(req1, adapt(), [](auto ec, auto){
      BOOST_CHECK_EQUAL(ec, boost::system::errc::errc_t::operation_canceled);
   });

   conn.async_run([&](auto ec){
      BOOST_TEST(!ec);
      conn.cancel(operation::exec);
   });

   ioc.run();
}

void test_quit2(bool coalesce)
{
   request req{{false, coalesce}};
   req.push("HELLO", 3);
   req.push("QUIT");

   net::io_context ioc;
   auto const endpoints = resolve();
   connection conn{ioc};
   net::connect(conn.next_layer(), endpoints);


   conn.async_exec(req, adapt(), [](auto ec, auto) {
      BOOST_TEST(!ec);
   });

   conn.async_run([](auto ec) {
      BOOST_TEST(!ec);
   });

   ioc.run();
}

BOOST_AUTO_TEST_CASE(test_quit)
{
   test_quit2(true);
   test_quit2(false);
}
