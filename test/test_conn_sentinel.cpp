//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/config.hpp>
#include <boost/redis/connection.hpp>
#include <boost/redis/ignore.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/response.hpp>

#include <boost/core/lightweight_test.hpp>

#include "common.hpp"

namespace net = boost::asio;
using namespace boost::redis;
using namespace std::chrono_literals;
using boost::system::error_code;

namespace {

// We can execute requests normally when using Sentinel run
void test_exec()
{
   // Setup
   net::io_context ioc;
   connection conn{ioc};

   config cfg;
   cfg.sentinel.addresses = {
      {"localhost", "26379"},
      {"localhost", "26380"},
      {"localhost", "26381"},
   };
   cfg.sentinel.master_name = "mymaster";

   // Verify that we're connected to the master, listening at port 6380
   request req;
   req.push("CLIENT", "INFO");

   response<std::string> resp;

   bool exec_finished = false, run_finished = false;

   conn.async_exec(req, resp, [&](error_code ec, std::size_t) {
      exec_finished = true;
      BOOST_TEST_EQ(ec, error_code());
      BOOST_TEST_EQ(find_client_info(std::get<0>(resp).value(), "laddr"), "127.0.0.1:6380");
      conn.cancel();
   });

   conn.async_run(cfg, [&](error_code ec) {
      run_finished = true;
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
   });

   ioc.run_for(test_timeout);

   BOOST_TEST(exec_finished);
   BOOST_TEST(run_finished);
}

// If connectivity to the Redis master fails, we can reconnect
void test_reconnect()
{
   // Setup
   net::io_context ioc;
   connection conn{ioc};

   config cfg;
   cfg.sentinel.addresses = {
      {"localhost", "26379"},
      {"localhost", "26380"},
      {"localhost", "26381"},
   };
   cfg.sentinel.master_name = "mymaster";

   // Will cause the connection to fail
   request req_quit;
   req_quit.push("QUIT");

   // Will succeed if the reconnection succeeds
   request req_ping;
   req_ping.push("PING", "sentinel_reconnect");
   req_ping.get_config().cancel_if_unresponded = false;

   bool quit_finished = false, ping_finished = false, run_finished = false;

   conn.async_exec(req_quit, ignore, [&](error_code ec1, std::size_t) {
      quit_finished = true;
      BOOST_TEST_EQ(ec1, error_code());
      conn.async_exec(req_ping, ignore, [&](error_code ec2, std::size_t) {
         ping_finished = true;
         BOOST_TEST_EQ(ec2, error_code());
         conn.cancel();
      });
   });

   conn.async_run(cfg, [&](error_code ec) {
      run_finished = true;
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
   });

   ioc.run_for(test_timeout);

   BOOST_TEST(quit_finished);
   BOOST_TEST(ping_finished);
   BOOST_TEST(run_finished);
}

}  // namespace

int main()
{
   test_exec();
   test_reconnect();

   return boost::report_errors();
}
