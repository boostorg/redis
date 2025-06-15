/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/response.hpp>

#include <boost/asio/error.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/system/error_code.hpp>

#include "common.hpp"

#include <cstddef>
#include <string>
#include <string_view>

using boost::system::error_code;
namespace net = boost::asio;
using namespace boost::redis;
using namespace std::chrono_literals;
using namespace std::string_view_literals;

namespace {

constexpr std::string_view unix_socket_path = "/tmp/redis-socks/redis.sock";

// Executing commands using UNIX sockets works
void test_exec()
{
   // Setup
   net::io_context ioc;
   connection conn{ioc};
   auto cfg = make_test_config();
   cfg.unix_socket = unix_socket_path;
   bool run_finished = false, exec_finished = false;

   // Run the connection
   conn.async_run(cfg, {}, [&run_finished](error_code ec) {
      run_finished = true;
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
   });

   // Execute a request
   request req;
   req.push("PING", "unix");
   response<std::string> res;
   conn.async_exec(req, res, [&](error_code ec, std::size_t) {
      exec_finished = true;
      BOOST_TEST_EQ(ec, error_code());
      conn.cancel();
   });

   // Run
   ioc.run_for(test_timeout);

   // Check
   BOOST_TEST(exec_finished);
   BOOST_TEST(run_finished);
   BOOST_TEST_EQ(std::get<0>(res).value(), "unix"sv);
}

// If the connection is lost when using a UNIX socket, we can reconnect
void test_reconnection()
{
   // Setup
   net::io_context ioc;
   net::steady_timer timer{ioc};
   connection conn{ioc};
   auto cfg = make_test_config();
   cfg.unix_socket = unix_socket_path;
   cfg.reconnect_wait_interval = 10ms;  // make the test run faster

   request ping_request;
   ping_request.push("PING", "some_value");

   request quit_request;
   quit_request.push("QUIT");

   bool exec_finished = false, run_finished = false;

   // Run the connection
   conn.async_run(cfg, {}, [&](error_code ec) {
      run_finished = true;
      BOOST_TEST(ec == net::error::operation_aborted);
   });

   // The PING is the end of the callback chain
   auto ping_callback = [&](error_code ec, std::size_t) {
      exec_finished = true;
      BOOST_TEST(ec == error_code());
      conn.cancel();
   };

   auto quit_callback = [&](error_code ec, std::size_t) {
      BOOST_TEST(ec == error_code());

      // If a request is issued immediately after QUIT, the request sometimes
      // fails, probably due to a race condition. This dispatches any pending
      // handlers, triggering the reconnection process.
      // TODO: this should not be required.
      ioc.poll();
      conn.async_exec(ping_request, ignore, ping_callback);
   };

   conn.async_exec(quit_request, ignore, quit_callback);

   ioc.run_for(test_timeout);

   BOOST_TEST(exec_finished);
   BOOST_TEST(run_finished);
}

// switch
// invalid config: tls
// invalid config: not supported

}  // namespace

int main()
{
   test_exec();
   test_reconnection();

   return boost::report_errors();
}