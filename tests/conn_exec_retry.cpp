/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <iostream>
#include <boost/asio.hpp>
#include <boost/system/errc.hpp>

#define BOOST_TEST_MODULE conn-exec-retry
#include <boost/test/included/unit_test.hpp>

#include <boost/redis.hpp>
#include <boost/redis/src.hpp>

#include "common.hpp"

namespace net = boost::asio;
using error_code = boost::system::error_code;
using connection = boost::redis::connection;
using boost::redis::operation;
using boost::redis::request;
using boost::redis::response;
using boost::redis::ignore;

BOOST_AUTO_TEST_CASE(request_retry_false)
{
   request req0;
   req0.get_config().cancel_on_connection_lost = true;
   req0.push("HELLO", 3);

   request req1;
   req1.get_config().cancel_on_connection_lost = true;
   req1.push("BLPOP", "any", 0);

   request req2;
   req2.get_config().cancel_on_connection_lost = false;
   req2.get_config().cancel_if_unresponded = true;
   req2.push("PING");

   net::io_context ioc;
   connection conn{ioc};

   net::steady_timer st{ioc};
   st.expires_after(std::chrono::seconds{1});
   st.async_wait([&](auto){
      // Cancels the request before receiving the response. This
      // should cause the third request to complete with error
      // although it has cancel_on_connection_lost = false. The reason
      // being it has already been written so
      // cancel_on_connection_lost does not apply.
      conn.cancel(operation::run);
   });

   auto const endpoints = resolve();
   net::connect(conn.next_layer(), endpoints);

   auto c2 = [&](auto ec, auto){
      BOOST_CHECK_EQUAL(ec, boost::system::errc::errc_t::operation_canceled);
   };

   auto c1 = [&](auto ec, auto){
      BOOST_CHECK_EQUAL(ec, boost::system::errc::errc_t::operation_canceled);
   };

   auto c0 = [&](auto ec, auto){
      BOOST_TEST(!ec);
      conn.async_exec(req1, ignore, c1);
      conn.async_exec(req2, ignore, c2);
   };

   conn.async_exec(req0, ignore, c0);

   conn.async_run([](auto ec){
      BOOST_CHECK_EQUAL(ec, boost::system::errc::errc_t::operation_canceled);
   });

   ioc.run();
}

BOOST_AUTO_TEST_CASE(request_retry_true)
{
   request req0;
   req0.get_config().cancel_on_connection_lost = true;
   req0.push("HELLO", 3);

   request req1;
   req1.get_config().cancel_on_connection_lost = true;
   req1.push("BLPOP", "any", 0);

   request req2;
   req2.get_config().cancel_on_connection_lost = false;
   req2.get_config().cancel_if_unresponded = false;
   req2.push("PING");

   request req3;
   req3.get_config().cancel_on_connection_lost = true;
   req3.get_config().cancel_if_unresponded = true;
   req3.push("QUIT");

   net::io_context ioc;
   connection conn{ioc};

   net::steady_timer st{ioc};
   st.expires_after(std::chrono::seconds{1});
   st.async_wait([&](auto){
      // Cancels the request before receiving the response. This
      // should cause the thrid request to not complete with error
      // since it has cancel_if_unresponded = true and cancellation commes
      // after it was written.
      conn.cancel(boost::redis::operation::run);
   });

   auto const endpoints = resolve();
   net::connect(conn.next_layer(), endpoints);

   auto c3 = [&](auto ec, auto){
      BOOST_TEST(!ec);
   };

   auto c2 = [&](auto ec, auto){
      BOOST_TEST(!ec);
      conn.async_exec(req3, ignore, c3);
   };

   auto c1 = [](auto ec, auto){
      BOOST_CHECK_EQUAL(ec, boost::system::errc::errc_t::operation_canceled);
   };

   auto c0 = [&](auto ec, auto){
      BOOST_TEST(!ec);
      conn.async_exec(req1, ignore, c1);
      conn.async_exec(req2, ignore, c2);
   };

   conn.async_exec(req0, ignore, c0);

   conn.async_run([&](auto ec){
      // The first cacellation.
      BOOST_CHECK_EQUAL(ec, boost::system::errc::errc_t::operation_canceled);
      conn.reset_stream();

      // Reconnects and runs again to test req3.
      net::connect(conn.next_layer(), endpoints);
      conn.async_run([&](auto ec){
         std::cout << ec.message() << std::endl;
         BOOST_TEST(!ec);
      });
   });

   ioc.run();
}
