/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>
#include <boost/redis/ignore.hpp>
#include <boost/redis/response.hpp>

#include <boost/asio/error.hpp>
#include <boost/core/lightweight_test.hpp>

#include "common.hpp"

#include <cstddef>

namespace net = boost::asio;
namespace redis = boost::redis;
using error_code = boost::system::error_code;
using connection = boost::redis::connection;
using boost::redis::request;
using boost::redis::ignore;
using namespace std::chrono_literals;

namespace {

// The health checker detects dead connections and triggers reconnection
void test_check_health_reconnection()
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

}  // namespace

int main()
{
   test_check_health_reconnection();

   return boost::report_errors();
}