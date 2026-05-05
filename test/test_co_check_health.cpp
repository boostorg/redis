//
// Copyright (c) 2026 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/co_connection.hpp>
#include <boost/redis/ignore.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/response.hpp>

#include <boost/capy/delay.hpp>
#include <boost/capy/error.hpp>
#include <boost/capy/ex/this_coro.hpp>
#include <boost/capy/task.hpp>
#include <boost/capy/when_any.hpp>
#include <boost/core/lightweight_test.hpp>

#include "common.hpp"
#include "corosio_common.hpp"

#include <chrono>
#include <string>
#include <system_error>

namespace capy = boost::capy;
using namespace boost::redis;
using namespace boost::redis::test;
using error_code = std::error_code;
using namespace std::chrono_literals;

namespace {

// The health checker detects dead connections and triggers reconnection
capy::task<void> test_reconnection()
{
   // Setup
   co_connection conn{co_await capy::this_coro::executor};

   auto exec_fn = [&]() -> capy::io_task<> {
      // This request will block forever, causing the connection to become unresponsive
      request req1;
      req1.push("BLPOP", "any", 0);

      // This request should be executed after reconnection
      request req2;
      req2.push("PING", "after_reconnection");
      req2.get_config().cancel_if_unresponded = false;
      req2.get_config().cancel_on_connection_lost = false;

      // This request will complete after the health checker deems the connection
      // as unresponsive and triggers a reconnection (it's configured to be cancelled
      // on connection lost).
      auto [ec1] = co_await conn.exec(req1, ignore);
      BOOST_TEST_EQ(ec1, capy_canceled_condition());

      // Execute the second request. This one will succeed after reconnection
      auto [ec2] = co_await conn.exec(req2, ignore);
      BOOST_TEST_EQ(ec2, error_code());

      co_return {};
   };

   auto run_fn = [&]() -> capy::io_task<> {
      // Make the test run faster
      auto cfg = make_test_config();
      cfg.health_check_interval = 500ms;

      auto [ec] = co_await conn.run(cfg);
      BOOST_TEST_EQ(ec, capy_canceled_condition());

      co_return {};
   };

   co_await capy::when_any(exec_fn(), run_fn());
}

// We use the correct error code when a ping times out
capy::task<void> test_error_code()
{
   co_connection conn{co_await capy::this_coro::executor};

   auto exec_fn = [&]() -> capy::io_task<> {
      // This request will block forever, causing the connection to become unresponsive
      request req;
      req.push("BLPOP", "any", 0);

      auto [ec] = co_await conn.exec(req, ignore);
      BOOST_TEST_EQ(ec, capy_canceled_condition());

      co_return {};
   };

   auto run_fn = [&]() -> capy::io_task<> {
      // Make the test run faster
      auto cfg = make_test_config();
      cfg.health_check_interval = 200ms;
      cfg.reconnect_wait_interval = 0s;

      auto [ec] = co_await conn.run(cfg);
      BOOST_TEST_EQ(ec, error::pong_timeout);

      co_return {};
   };

   co_await capy::when_any(exec_fn(), run_fn());
}

// A ping interval of zero disables timeouts (and doesn't cause trouble)
capy::task<void> test_disabled()
{
   co_connection conn{co_await capy::this_coro::executor};

   auto exec_fn = [&]() -> capy::io_task<> {
      // Run a couple of requests to verify that the connection works fine
      request req1;
      req1.push("PING", "health_check_disabled_1");

      request req2;
      req1.push("PING", "health_check_disabled_2");

      auto [ec1] = co_await conn.exec(req1, ignore);
      BOOST_TEST_EQ(ec1, std::error_code());

      auto [ec2] = co_await conn.exec(req1, ignore);
      BOOST_TEST_EQ(ec2, std::error_code());

      co_return {};
   };

   auto run_fn = [&]() -> capy::io_task<> {
      auto cfg = make_test_config();
      cfg.health_check_interval = 0s;
      auto [ec] = co_await conn.run(cfg);
      BOOST_TEST_EQ(ec, capy_canceled_condition());

      co_return {};
   };

   co_await capy::when_any(exec_fn(), run_fn());
}

// Generates a sufficiently unique name for channels so
// tests may be run in parallel for different configurations
std::string make_unique_id()
{
   auto t = std::chrono::high_resolution_clock::now();
   return "test-flexible-health-checks-" + std::to_string(t.time_since_epoch().count());
}

// Receiving data is sufficient to consider our connection healthy.
// Sends a blocking request that causes PINGs to not be answered,
// and subscribes to a channel to receive pushes periodically.
// This simulates situations of heavy load, where PINGs may not be answered on time.
capy::task<void> test_flexible()
{
   // Setup
   co_connection conn1{co_await capy::this_coro::executor};
   co_connection conn2{co_await capy::this_coro::executor};
   auto cfg = make_test_config();
   cfg.health_check_interval = 500ms;
   std::string channel_name = make_unique_id();

   auto run1_fn = [&]() -> capy::io_task<> {
      auto [ec] = co_await conn1.run(cfg);
      BOOST_TEST_EQ(ec, capy_canceled_condition());
      co_return {};
   };

   auto run2_fn = [&]() -> capy::io_task<> {
      auto [ec] = co_await conn2.run(cfg);
      BOOST_TEST_EQ(ec, capy_canceled_condition());
      co_return {};
   };

   auto exec_fn = [&]() -> capy::io_task<> {
      // This request will block for much longer than the health check
      // interval. If we weren't receiving pushes, the connection would be considered dead.
      // If this request finishes successfully, the health checker is doing good
      request blocking_req;
      blocking_req.push("SUBSCRIBE", channel_name);
      blocking_req.push("BLPOP", "any", 2);
      blocking_req.get_config().cancel_if_unresponded = true;
      blocking_req.get_config().cancel_on_connection_lost = true;

      // BLPOP will return NIL, so we can't use ignore
      generic_response resp;
      auto [ec] = co_await conn1.exec(blocking_req, resp);
      BOOST_TEST_EQ(ec, error_code());

      co_return {};
   };

   auto publish_fn = [&]() -> capy::io_task<> {
      request publish_req;
      publish_req.push("PUBLISH", channel_name, "test_health_check_flexible");

      while (true) {
         // Publish a message
         auto [ec] = co_await conn2.exec(publish_req, ignore);
         if (ec == capy_canceled_condition())
            co_return {};
         BOOST_TEST_EQ(ec, error_code());

         // Wait for some time and publish again
         auto [ec2] = co_await capy::delay(100ms);
         if (ec2 == capy_canceled_condition())
            co_return {};
         BOOST_TEST_EQ(ec2, error_code());
      }
   };

   co_await capy::when_any(run1_fn(), run2_fn(), exec_fn(), publish_fn());
}

}  // namespace

int main()
{
   run_coroutine_test(test_reconnection());
   run_coroutine_test(test_error_code());
   run_coroutine_test(test_disabled());
   run_coroutine_test(test_flexible());

   return boost::report_errors();
}