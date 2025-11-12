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

#include <boost/asio/ssl/context.hpp>
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

// If a Sentinel is not reachable, we try the next one
void test_sentinel_not_reachable()
{
   // Setup
   net::io_context ioc;
   connection conn{ioc};

   config cfg;
   cfg.sentinel.addresses = {
      {"localhost", "45678"}, // invalid
      {"localhost", "26381"},
   };
   cfg.sentinel.master_name = "mymaster";

   // Verify that we're connected to the master, listening at port 6380
   request req;
   req.push("PING", "test_sentinel_not_reachable");

   bool exec_finished = false, run_finished = false;

   conn.async_exec(req, ignore, [&](error_code ec, std::size_t) {
      exec_finished = true;
      BOOST_TEST_EQ(ec, error_code());
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

// Both Sentinels and masters may be protected with authorization
void test_auth()
{
   // Setup
   net::io_context ioc;
   connection conn{ioc};

   config cfg;
   cfg.sentinel.addresses = {
      {"localhost", "26379"},
   };
   cfg.sentinel.master_name = "mymaster";
   cfg.sentinel.setup.push("HELLO", 3, "AUTH", "sentinel_user", "sentinel_pass");

   cfg.use_setup = true;
   cfg.setup.clear();
   cfg.setup.push("HELLO", 3, "AUTH", "redis_user", "redis_pass");

   // Verify that we're authenticated correctly
   request req;
   req.push("ACL", "WHOAMI");

   response<std::string> resp;

   bool exec_finished = false, run_finished = false;

   conn.async_exec(req, resp, [&](error_code ec, std::size_t) {
      exec_finished = true;
      BOOST_TEST_EQ(ec, error_code());
      BOOST_TEST(std::get<0>(resp).has_value());
      BOOST_TEST_EQ(std::get<0>(resp).value(), "redis_user");
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

// TLS might be used with Sentinels. In our setup, nodes don't use TLS,
// but this setting is independent from Sentinel.
void test_tls()
{
   // Setup
   net::io_context ioc;
   net::ssl::context ssl_ctx{net::ssl::context::tlsv13_client};

   // The custom server uses a certificate signed by a CA
   // that is not trusted by default - skip verification.
   ssl_ctx.set_verify_mode(net::ssl::verify_none);

   connection conn{ioc, std::move(ssl_ctx)};

   config cfg;
   cfg.sentinel.addresses = {
      {"localhost", "36379"},
      {"localhost", "36380"},
      {"localhost", "36381"},
   };
   cfg.sentinel.master_name = "mymaster";
   cfg.sentinel.use_ssl = true;

   request req;
   req.push("PING", "test_sentinel_tls");

   bool exec_finished = false, run_finished = false;

   conn.async_exec(req, ignore, [&](error_code ec, std::size_t) {
      exec_finished = true;
      BOOST_TEST(ec == error_code());
      conn.cancel();
   });

   conn.async_run(cfg, {}, [&](error_code ec) {
      run_finished = true;
      BOOST_TEST(ec == net::error::operation_aborted);
   });

   ioc.run_for(test_timeout);

   BOOST_TEST(exec_finished);
   BOOST_TEST(run_finished);
}

// We can also connect to replicas
void test_replica()
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
   cfg.sentinel.server_role = role::replica;

   // Verify that we're connected to a replica
   request req;
   req.push("ROLE");

   generic_response resp;

   bool exec_finished = false, run_finished = false;

   conn.async_exec(req, resp, [&](error_code ec, std::size_t) {
      exec_finished = true;
      BOOST_TEST_EQ(ec, error_code());

      // ROLE outputs an array, 1st element should be 'slave'
      BOOST_TEST(resp.has_value());
      BOOST_TEST_GE(resp.value().size(), 2u);
      BOOST_TEST_EQ(resp.value().at(1u).value, "slave");

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

// If no Sentinel is reachable, an error is issued.
// This tests disabling reconnection with Sentinel, too.
void test_error_no_sentinel_reachable()
{
   // Setup
   net::io_context ioc;
   connection conn{ioc};

   config cfg;
   cfg.sentinel.addresses = {
      {"localhost", "43210"},
      {"localhost", "43211"},
   };
   cfg.sentinel.master_name = "mymaster";
   cfg.reconnect_wait_interval = 0s;  // disable reconnection so we can verify the error

   bool run_finished = false;

   conn.async_run(cfg, [&](error_code ec) {
      run_finished = true;
      BOOST_TEST_EQ(ec, error::no_sentinel_reachable);
   });

   ioc.run_for(test_timeout);

   BOOST_TEST(run_finished);
}

// If Sentinel doesn't know about the configured master,
// the appropriate error is returned
void test_error_unknown_master()
{
   // Setup
   net::io_context ioc;
   connection conn{ioc};

   config cfg;
   cfg.sentinel.addresses = {
      {"localhost", "26380"},
   };
   cfg.sentinel.master_name = "unknown_master";
   cfg.reconnect_wait_interval = 0s;  // disable reconnection so we can verify the error

   bool run_finished = false;

   conn.async_run(cfg, [&](error_code ec) {
      run_finished = true;
      BOOST_TEST_EQ(ec, error::no_sentinel_reachable);
   });

   ioc.run_for(test_timeout);

   BOOST_TEST(run_finished);
}

// The same applies when connecting to replicas, too
void test_error_unknown_master_replica()
{
   // Setup
   net::io_context ioc;
   connection conn{ioc};

   config cfg;
   cfg.sentinel.addresses = {
      {"localhost", "26380"},
   };
   cfg.sentinel.master_name = "unknown_master";
   cfg.reconnect_wait_interval = 0s;  // disable reconnection so we can verify the error
   cfg.sentinel.server_role = role::replica;

   bool run_finished = false;

   conn.async_run(cfg, [&](error_code ec) {
      run_finished = true;
      BOOST_TEST_EQ(ec, error::no_sentinel_reachable);
   });

   ioc.run_for(test_timeout);

   BOOST_TEST(run_finished);
}

}  // namespace

int main()
{
   // Create the required users in the master, replicas and sentinels
   create_user("6380", "redis_user", "redis_pass");
   create_user("6381", "redis_user", "redis_pass");
   create_user("6382", "redis_user", "redis_pass");
   create_user("26379", "sentinel_user", "sentinel_pass");
   create_user("26380", "sentinel_user", "sentinel_pass");
   create_user("26381", "sentinel_user", "sentinel_pass");

   // Actual tests
   test_exec();
   test_reconnect();
   test_sentinel_not_reachable();
   test_auth();
   test_tls();
   test_replica();

   test_error_no_sentinel_reachable();
   test_error_unknown_master();
   test_error_unknown_master_replica();

   return boost::report_errors();
}
