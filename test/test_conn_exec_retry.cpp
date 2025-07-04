/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>

#include <boost/system/error_code.hpp>

#include <cstddef>

#define BOOST_TEST_MODULE conn_exec_retry
#include <boost/test/included/unit_test.hpp>

#include "common.hpp"

#include <iostream>

namespace net = boost::asio;
using error_code = boost::system::error_code;
using connection = boost::redis::connection;
using boost::redis::operation;
using boost::redis::request;
using boost::redis::response;
using boost::redis::ignore;
using boost::redis::logger;
using boost::redis::config;
using namespace std::chrono_literals;

namespace {

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
   auto conn = std::make_shared<connection>(ioc);

   net::steady_timer st{ioc};

   bool timer_finished = false, c2_called = false, c1_called = false, c0_called = false,
        run_finished = false;

   st.expires_after(std::chrono::seconds{1});
   st.async_wait([&](error_code ec) {
      // Cancels the request before receiving the response. This
      // should cause the third request to complete with error
      // although it has cancel_on_connection_lost = false. The reason
      // being it has already been written so
      // cancel_on_connection_lost does not apply.
      timer_finished = true;
      BOOST_TEST(ec == error_code());
      conn->cancel(operation::run);
      conn->cancel(operation::reconnection);
      std::cout << "async_wait" << std::endl;
   });

   auto c2 = [&](error_code ec, std::size_t) {
      c2_called = true;
      std::cout << "c2" << std::endl;
      BOOST_TEST(ec == net::error::operation_aborted);
   };

   auto c1 = [&](error_code ec, std::size_t) {
      c1_called = true;
      std::cout << "c1" << std::endl;
      BOOST_TEST(ec == net::error::operation_aborted);
   };

   auto c0 = [&](error_code ec, std::size_t) {
      c0_called = true;
      std::cout << "c0" << std::endl;
      BOOST_TEST(ec == error_code());
      conn->async_exec(req1, ignore, c1);
      conn->async_exec(req2, ignore, c2);
   };

   conn->async_exec(req0, ignore, c0);

   auto cfg = make_test_config();
   conn->async_run(cfg, {boost::redis::logger::level::debug}, [&](error_code ec) {
      run_finished = true;
      std::cout << "async_run: " << ec.message() << std::endl;
      conn->cancel();
   });

   ioc.run_for(test_timeout);

   BOOST_TEST(timer_finished);
   BOOST_TEST(c0_called);
   BOOST_TEST(c1_called);
   BOOST_TEST(c2_called);
   BOOST_TEST(run_finished);
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
   auto conn = std::make_shared<connection>(ioc);

   net::steady_timer st{ioc};

   bool timer_finished = false, c0_called = false, c1_called = false, c2_called = false,
        c3_called = false, run_finished = false;

   st.expires_after(std::chrono::seconds{1});
   st.async_wait([&](error_code ec) {
      // Cancels the request before receiving the response. This
      // should cause the third request to not complete with error
      // since it has cancel_if_unresponded = true and cancellation
      // comes after it was written.
      timer_finished = true;
      BOOST_TEST(ec == error_code());
      conn->cancel(operation::run);
   });

   auto c3 = [&](error_code ec, std::size_t) {
      c3_called = true;
      std::cout << "c3: " << ec.message() << std::endl;
      BOOST_TEST(ec == error_code());
      conn->cancel();
   };

   auto c2 = [&](error_code ec, std::size_t) {
      c2_called = true;
      BOOST_TEST(ec == error_code());
      conn->async_exec(req3, ignore, c3);
   };

   auto c1 = [&](error_code ec, std::size_t) {
      c1_called = true;
      BOOST_TEST(ec == net::error::operation_aborted);
   };

   auto c0 = [&](error_code ec, std::size_t) {
      c0_called = true;
      BOOST_TEST(ec == error_code());
      conn->async_exec(req1, ignore, c1);
      conn->async_exec(req2, ignore, c2);
   };

   conn->async_exec(req0, ignore, c0);

   auto cfg = make_test_config();
   cfg.health_check_interval = 5s;
   conn->async_run(cfg, {}, [&](error_code ec) {
      run_finished = true;
      std::cout << ec.message() << std::endl;
      BOOST_TEST(ec != error_code());
   });

   ioc.run_for(test_timeout);

   BOOST_TEST(timer_finished);
   BOOST_TEST(c0_called);
   BOOST_TEST(c1_called);
   BOOST_TEST(c2_called);
   BOOST_TEST(c3_called);
   BOOST_TEST(run_finished);
}

}  // namespace