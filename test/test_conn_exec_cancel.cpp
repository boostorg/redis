/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>
#include <boost/redis/ignore.hpp>
#include <boost/redis/request.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/system/errc.hpp>

#include "common.hpp"

#include <cstddef>
#include <iostream>

using namespace std::chrono_literals;

namespace net = boost::asio;
using error_code = boost::system::error_code;
using boost::redis::operation;
using boost::redis::error;
using boost::redis::request;
using boost::redis::response;
using boost::redis::generic_response;
using boost::redis::ignore;
using boost::redis::ignore_t;
using boost::redis::logger;
using boost::redis::connection;
using namespace std::chrono_literals;

// TODO: replace this by connection once it supports asio::cancel_after
// See https://github.com/boostorg/redis/issues/226
using connection_type = boost::redis::basic_connection<net::any_io_executor>;

namespace {

// We can cancel requests that haven't been written yet.
// All cancellation types are supported here.
void test_cancel_pending()
{
   struct {
      const char* name;
      net::cancellation_type_t cancel_type;
   } test_cases[] = {
      {"terminal", net::cancellation_type_t::terminal},
      {"partial",  net::cancellation_type_t::partial },
      {"total",    net::cancellation_type_t::total   },
   };

   for (const auto& tc : test_cases) {
      std::cerr << "Running test case: " << tc.name << std::endl;

      // Setup
      net::io_context ctx;
      connection conn(ctx);
      request req;
      req.push("get", "mykey");

      // Issue a request without calling async_run(), so the request stays waiting forever
      net::cancellation_signal sig;
      bool called = false;
      conn.async_exec(
         req,
         ignore,
         net::bind_cancellation_slot(sig.slot(), [&](error_code ec, std::size_t sz) {
            BOOST_TEST_EQ(ec, net::error::operation_aborted);
            BOOST_TEST_EQ(sz, 0u);
            called = true;
         }));

      // Issue a cancellation
      sig.emit(tc.cancel_type);

      // Prevent the test for deadlocking in case of failure
      ctx.run_for(test_timeout);
      BOOST_TEST(called);
   }
}

// We can safely cancel requests that have been written but which
// responses haven't been received yet.
void test_cancel_written()
{
   // Setup
   net::io_context ctx;
   connection_type conn{ctx};
   auto cfg = make_test_config();
   cfg.health_check_interval = std::chrono::seconds::zero();
   bool run_finished = false, exec1_finished = false, exec2_finished = false;

   // Will be cancelled after it has been written but before the
   // response arrives.
   request req1;
   req1.push("BLPOP", "any", 1);

   // Will finish successfully once the response to the BLPOP arrives
   request req2;
   req2.push("PING", "after_blpop");

   // Run the connection
   conn.async_run(cfg, [&](error_code ec) {
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
      run_finished = true;
   });

   // The request will be cancelled before it receives a response.
   // Our BLPOP will wait for longer than the timeout we're using.
   auto blpop_cb = [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
      exec1_finished = true;
   };
   conn.async_exec(req1, ignore, net::cancel_after(500ms, blpop_cb));

   // The PING will be sent after the BLPOP because it's been scheduled after it.
   // The response will be received after the BLPOP, but it will be processed successfully.
   conn.async_exec(req2, ignore, [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, error_code());
      conn.cancel();
      exec2_finished = true;
   });

   ctx.run_for(test_timeout);
   BOOST_TEST(run_finished);
   BOOST_TEST(exec1_finished);
   BOOST_TEST(exec2_finished);
}

void test_cancel_of_req_written_on_run_canceled()
{
   net::io_context ioc;
   auto conn = std::make_shared<connection>(ioc);

   request req0;
   req0.push("PING");

   // Sends a request that will be blocked forever, so we can test
   // canceling it while waiting for a response.
   request req1;
   req1.get_config().cancel_on_connection_lost = true;
   req1.get_config().cancel_if_unresponded = true;
   req1.push("BLPOP", "any", 0);

   bool finished = false;

   auto c1 = [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
      finished = true;
   };

   auto c0 = [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, error_code());
      conn->async_exec(req1, ignore, c1);
   };

   conn->async_exec(req0, ignore, c0);

   auto cfg = make_test_config();
   cfg.health_check_interval = std::chrono::seconds{5};
   run(conn);

   net::steady_timer st{ioc};
   st.expires_after(std::chrono::seconds{1});
   st.async_wait([&](error_code ec) {
      BOOST_TEST_EQ(ec, error_code());
      conn->cancel(operation::run);
      conn->cancel(operation::reconnection);
   });

   ioc.run_for(test_timeout);
   BOOST_TEST(finished);
}

}  // namespace

int main()
{
   test_cancel_pending();
   test_cancel_written();
   test_cancel_of_req_written_on_run_canceled();

   return boost::report_errors();
}
