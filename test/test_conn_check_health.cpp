/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>
#include <boost/redis/ignore.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/response.hpp>

#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/core/lightweight_test.hpp>

#include "common.hpp"

#include <cstddef>
#include <string>

namespace net = boost::asio;
namespace redis = boost::redis;
using error_code = boost::system::error_code;
using connection = boost::redis::connection;
using boost::redis::request;
using boost::redis::ignore;
using namespace std::chrono_literals;

namespace {

// The health checker detects dead connections and triggers reconnection
void test_reconnection()
{
   // Setup
   net::io_context ioc;
   connection conn{ioc};

   // This request will block forever, causing the connection to become unresponsive
   request req1;
   req1.push("BLPOP", "any", 0);

   // This request should be executed after reconnection
   request req2;
   req2.push("PING", "after_reconnection");
   req2.get_config().cancel_if_unresponded = false;
   req2.get_config().cancel_on_connection_lost = false;

   // Make the test run faster
   auto cfg = make_test_config();
   cfg.health_check_interval = 500ms;
   cfg.reconnect_wait_interval = 100ms;

   bool run_finished = false, exec1_finished = false, exec2_finished = false;

   conn.async_run(cfg, [&](error_code ec) {
      run_finished = true;
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
   });

   // This request will complete after the health checker deems the connection
   // as unresponsive and triggers a reconnection (it's configured to be cancelled
   // on connection lost).
   conn.async_exec(req1, ignore, [&](error_code ec, std::size_t) {
      exec1_finished = true;
      BOOST_TEST_EQ(ec, net::error::operation_aborted);

      // Execute the second request. This one will succeed after reconnection
      conn.async_exec(req2, ignore, [&](error_code ec2, std::size_t) {
         exec2_finished = true;
         BOOST_TEST_EQ(ec2, error_code());
         conn.cancel();
      });
   });

   ioc.run_for(test_timeout);

   BOOST_TEST(run_finished);
   BOOST_TEST(exec1_finished);
   BOOST_TEST(exec2_finished);
}

// We use the correct error code when a ping times out
void test_error_code()
{
   // Setup
   net::io_context ioc;
   connection conn{ioc};

   // This request will block forever, causing the connection to become unresponsive
   request req;
   req.push("BLPOP", "any", 0);

   // Make the test run faster
   auto cfg = make_test_config();
   cfg.health_check_interval = 200ms;
   cfg.reconnect_wait_interval = 0s;

   bool run_finished = false, exec_finished = false;

   conn.async_run(cfg, [&](error_code ec) {
      run_finished = true;
      BOOST_TEST_EQ(ec, boost::redis::error::pong_timeout);
   });

   // This request will complete after the health checker deems the connection
   // as unresponsive and triggers a reconnection (it's configured to be cancelled
   // if unresponded).
   conn.async_exec(req, ignore, [&](error_code ec, std::size_t) {
      exec_finished = true;
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
   });

   ioc.run_for(test_timeout);

   BOOST_TEST(run_finished);
   BOOST_TEST(exec_finished);
}

// A ping interval of zero disables timeouts (and doesn't cause trouble)
void test_disabled()
{
   // Setup
   net::io_context ioc;
   connection conn{ioc};

   // Run a couple of requests to verify that the connection works fine
   request req1;
   req1.push("PING", "health_check_disabled_1");

   request req2;
   req1.push("PING", "health_check_disabled_2");

   auto cfg = make_test_config();
   cfg.health_check_interval = 0s;

   bool run_finished = false, exec1_finished = false, exec2_finished = false;

   conn.async_run(cfg, [&](error_code ec) {
      run_finished = true;
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
   });

   conn.async_exec(req1, ignore, [&](error_code ec, std::size_t) {
      exec1_finished = true;
      BOOST_TEST_EQ(ec, error_code());
      conn.async_exec(req2, ignore, [&](error_code ec2, std::size_t) {
         exec2_finished = true;
         BOOST_TEST_EQ(ec2, error_code());
         conn.cancel();
      });
   });

   ioc.run_for(test_timeout);

   BOOST_TEST(run_finished);
   BOOST_TEST(exec1_finished);
   BOOST_TEST(exec2_finished);
}

class test_flexible {
   net::io_context ioc;
   net::steady_timer timer{ioc};
   connection conn1{ioc}, conn2{ioc};
   request req_publish;
   bool exec_finished{false}, publisher_finished{false};

   void start_generating_traffic()
   {
      conn2.async_exec(req_publish, ignore, [this](error_code ec, std::size_t) {
         BOOST_TEST_EQ(ec, error_code());

         if (exec_finished) {
            conn2.cancel();
            publisher_finished = true;
         } else {
            timer.expires_after(100ms);
            timer.async_wait([this](error_code ec) {
               BOOST_TEST_EQ(ec, error_code());
               start_generating_traffic();
            });
         }
      });
   }

public:
   test_flexible() = default;

   void run()
   {
      auto cfg = make_test_config();
      cfg.health_check_interval = 500ms;
      boost::redis::generic_response resp;

      bool run1_finished = false, run2_finished = false;

      std::string channel_name = "abc";  // TODO: make this unique
      req_publish.push("PUBLISH", channel_name, "test_health_check_flexible");

      request req1;
      req1.push("SUBSCRIBE", channel_name);
      req1.push("BLPOP", "any", 2);
      req1.get_config().cancel_if_unresponded = true;
      req1.get_config().cancel_on_connection_lost = true;

      conn1.async_run(cfg, [&](error_code ec) {
         run1_finished = true;
         BOOST_TEST_EQ(ec, net::error::operation_aborted);
      });

      conn2.async_run(cfg, [&](error_code ec) {
         run2_finished = true;
         BOOST_TEST_EQ(ec, net::error::operation_aborted);
      });

      conn1.async_exec(req1, resp, [&](error_code ec, std::size_t) {
         exec_finished = true;
         BOOST_TEST_EQ(ec, error_code());
         conn1.cancel();
      });

      start_generating_traffic();

      ioc.run_for(test_timeout);

      BOOST_TEST(run1_finished);
      BOOST_TEST(run2_finished);
      BOOST_TEST(exec_finished);
   }
};

}  // namespace

int main()
{
   test_reconnection();
   test_error_code();
   test_disabled();
   test_flexible().run();

   return boost::report_errors();
}