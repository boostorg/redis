//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/co_connection.hpp>
#include <boost/redis/config.hpp>
#include <boost/redis/error.hpp>
#include <boost/redis/ignore.hpp>
#include <boost/redis/logger.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/resp3/node.hpp>
#include <boost/redis/resp3/tree.hpp>
#include <boost/redis/resp3/type.hpp>
#include <boost/redis/response.hpp>

#include <boost/capy/ex/this_coro.hpp>
#include <boost/capy/io_task.hpp>
#include <boost/capy/task.hpp>
#include <boost/capy/when_all.hpp>
#include <boost/capy/when_any.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/corosio/tls_context.hpp>

#include "common.hpp"
#include "corosio_common.hpp"
#include "print_node.hpp"

#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace capy = boost::capy;
namespace corosio = boost::corosio;
using namespace boost::redis;
using namespace boost::redis::test;
using namespace std::chrono_literals;
using error_code = std::error_code;

namespace {

// RP TODO: this is duplicate
// Connects to the Redis server at the given port and creates a user
capy::task<void> create_user(
   std::string_view port,
   std::string_view username,
   std::string_view password)
{
   co_connection conn{co_await capy::this_coro::executor};

   auto exec_fn = [&]() -> capy::io_task<> {
      // Enable the user and grant them permissions on everything
      request req;
      req.push("ACL", "SETUSER", username, "on", ">" + std::string(password), "~*", "&*", "+@all");

      auto [ec] = co_await conn.exec(req, ignore);
      BOOST_TEST_EQ(ec, error_code());

      co_return {};
   };

   auto run_fn = [&]() -> capy::io_task<> {
      config cfg;
      cfg.addr.port = port;

      auto [ec] = co_await conn.run(cfg);
      BOOST_TEST_EQ(ec, canceled_condition());

      co_return {};
   };

   auto result = co_await capy::when_any(exec_fn(), run_fn());
   BOOST_TEST_EQ(result.index(), 1u);  // Exec finished 1st
}

config make_sentinel_config()
{
   config cfg;
   cfg.sentinel.addresses = {
      {"localhost", "26379"},
      {"localhost", "26380"},
      {"localhost", "26381"},
   };
   cfg.sentinel.master_name = "mymaster";
   return cfg;
}

// We can execute requests normally when using Sentinel
capy::task<> test_exec()
{
   co_connection conn{co_await capy::this_coro::executor};

   generic_response resp;

   auto exec_fn = [&]() -> capy::io_task<> {
      // Verify that we're connected to the master
      request req;
      req.push("ROLE");

      auto [ec] = co_await conn.exec(req, resp);
      BOOST_TEST_EQ(ec, error_code());

      // ROLE outputs an array, 1st element should be 'master'
      BOOST_TEST(resp.has_value());
      BOOST_TEST_GE(resp.value().size(), 2u);
      BOOST_TEST_EQ(resp.value().at(1u).value, "master");

      co_return {};
   };

   auto run_fn = [&]() -> capy::io_task<> {
      auto [ec] = co_await conn.run(make_sentinel_config());
      BOOST_TEST_EQ(ec, canceled_condition());
      co_return {};
   };

   auto result = co_await capy::when_any(exec_fn(), run_fn());
   BOOST_TEST_EQ(result.index(), 1u);  // Exec finished 1st
}

// We can use receive normally when using Sentinel
capy::task<> test_receive()
{
   co_connection conn{co_await capy::this_coro::executor};
   resp3::tree resp;
   conn.set_receive_response(resp);

   auto exec_fn = [&]() -> capy::io_task<> {
      // Subscribe to a channel. This produces a push message on itself
      request req;
      req.subscribe({"sentinel_channel"});
      auto [ec] = co_await conn.exec(req, resp);
      BOOST_TEST_EQ(ec, error_code());
      co_return {};
   };

   auto receive_fn = [&]() -> capy::io_task<> {
      auto [ec] = co_await conn.receive();
      BOOST_TEST_EQ(ec, error_code());
      co_return {};
   };

   auto work_fn = [&]() -> capy::io_task<> {
      auto [ec, a, b] = co_await capy::when_all(exec_fn(), receive_fn());
      BOOST_TEST_EQ(ec, error_code());
      co_return {};
   };

   auto run_fn = [&]() -> capy::io_task<> {
      auto [ec] = co_await conn.run(make_sentinel_config());
      BOOST_TEST_EQ(ec, canceled_condition());
      co_return {};
   };

   auto result = co_await capy::when_any(work_fn(), run_fn());
   BOOST_TEST_EQ(result.index(), 1u);  // Work finished 1st

   // We subscribed to channel 'sentinel_channel', and have 1 active subscription
   const resp3::node expected[] = {
      {resp3::type::push,        3u, 0u, ""                },
      {resp3::type::blob_string, 1u, 1u, "subscribe"       },
      {resp3::type::blob_string, 1u, 1u, "sentinel_channel"},
      {resp3::type::number,      1u, 1u, "1"               },
   };

   BOOST_TEST_ALL_EQ(resp.begin(), resp.end(), std::begin(expected), std::end(expected));
}

// If connectivity to the Redis master fails, we can reconnect
capy::task<> test_reconnect()
{
   co_connection conn{co_await capy::this_coro::executor};

   auto exec_fn = [&]() -> capy::io_task<> {
      // Will cause the connection to fail
      request req_quit;
      req_quit.push("QUIT");
      auto [ec1] = co_await conn.exec(req_quit, ignore);
      BOOST_TEST_EQ(ec1, error_code());

      // Will succeed if the reconnection succeeds
      request req_ping;
      req_ping.push("PING", "sentinel_reconnect");
      req_ping.get_config().cancel_if_unresponded = false;
      auto [ec2] = co_await conn.exec(req_ping, ignore);
      BOOST_TEST_EQ(ec2, error_code());

      co_return {};
   };

   auto run_fn = [&]() -> capy::io_task<> {
      auto [ec] = co_await conn.run(make_sentinel_config());
      BOOST_TEST_EQ(ec, canceled_condition());
      co_return {};
   };

   auto result = co_await capy::when_any(exec_fn(), run_fn());
   BOOST_TEST_EQ(result.index(), 1u);  // Exec finished 1st
}

// If a Sentinel is not reachable, we try the next one
capy::task<> test_sentinel_not_reachable()
{
   co_connection conn{co_await capy::this_coro::executor};

   config cfg;
   cfg.sentinel.addresses = {
      {"localhost", "45678"}, // invalid
      {"localhost", "26381"},
   };
   cfg.sentinel.master_name = "mymaster";

   auto exec_fn = [&]() -> capy::io_task<> {
      // Verify that we're connected to the master
      request req;
      req.push("PING", "test_sentinel_not_reachable");
      auto [ec] = co_await conn.exec(req, ignore);
      BOOST_TEST_EQ(ec, error_code());
      co_return {};
   };

   auto run_fn = [&]() -> capy::io_task<> {
      auto [ec] = co_await conn.run(cfg);
      BOOST_TEST_EQ(ec, canceled_condition());
      co_return {};
   };

   auto result = co_await capy::when_any(exec_fn(), run_fn());
   BOOST_TEST_EQ(result.index(), 1u);  // Exec finished 1st
}

// Both Sentinels and masters may be protected with authorization
capy::task<> test_auth()
{
   co_connection conn{co_await capy::this_coro::executor};

   config cfg;
   cfg.sentinel.addresses = {
      {"localhost", "26379"},
   };
   cfg.sentinel.master_name = "mymaster";
   cfg.sentinel.setup.push("HELLO", 3, "AUTH", "sentinel_user", "sentinel_pass");

   cfg.use_setup = true;
   cfg.setup.clear();
   cfg.setup.push("HELLO", 3, "AUTH", "redis_user", "redis_pass");

   auto exec_fn = [&]() -> capy::io_task<> {
      // Verify that we're authenticated correctly
      request req;
      req.push("ACL", "WHOAMI");
      response<std::string> resp;

      auto [ec] = co_await conn.exec(req, resp);
      BOOST_TEST_EQ(ec, error_code());
      BOOST_TEST(std::get<0>(resp).has_value());
      BOOST_TEST_EQ(std::get<0>(resp).value(), "redis_user");

      co_return {};
   };

   auto run_fn = [&]() -> capy::io_task<> {
      auto [ec] = co_await conn.run(cfg);
      BOOST_TEST_EQ(ec, canceled_condition());
      co_return {};
   };

   auto result = co_await capy::when_any(exec_fn(), run_fn());
   BOOST_TEST_EQ(result.index(), 1u);  // Exec finished 1st
}

// TLS might be used with Sentinels. In our setup, nodes don't use TLS,
// but this setting is independent from Sentinel.
capy::task<> test_tls()
{
   // The custom server uses a certificate signed by a CA
   // that is not trusted by default - skip verification.
   corosio::tls_context tls_ctx;
   tls_ctx.set_verify_mode(corosio::tls_verify_mode::none);

   co_connection conn{co_await capy::this_coro::executor, std::move(tls_ctx)};

   config cfg;
   cfg.sentinel.addresses = {
      {"localhost", "36379"},
      {"localhost", "36380"},
      {"localhost", "36381"},
   };
   cfg.sentinel.master_name = "mymaster";
   cfg.sentinel.use_ssl = true;

   auto exec_fn = [&]() -> capy::io_task<> {
      request req;
      req.push("PING", "test_sentinel_tls");
      auto [ec] = co_await conn.exec(req, ignore);
      BOOST_TEST_EQ(ec, error_code());
      co_return {};
   };

   auto run_fn = [&]() -> capy::io_task<> {
      auto [ec] = co_await conn.run(cfg);
      BOOST_TEST_EQ(ec, canceled_condition());
      co_return {};
   };

   auto result = co_await capy::when_any(exec_fn(), run_fn());
   BOOST_TEST_EQ(result.index(), 1u);  // Exec finished 1st
}

// We can also connect to replicas
capy::task<> test_replica()
{
   co_connection conn{co_await capy::this_coro::executor};

   auto exec_fn = [&]() -> capy::io_task<> {
      // Verify that we're connected to a replica
      request req;
      req.push("ROLE");
      generic_response resp;
      auto [ec] = co_await conn.exec(req, resp);
      BOOST_TEST_EQ(ec, error_code());

      // ROLE outputs an array, 1st element should be 'slave'
      BOOST_TEST(resp.has_value());
      BOOST_TEST_GE(resp.value().size(), 2u);
      BOOST_TEST_EQ(resp.value().at(1u).value, "slave");

      co_return {};
   };

   auto run_fn = [&]() -> capy::io_task<> {
      auto cfg = make_sentinel_config();
      cfg.sentinel.server_role = role::replica;

      auto [ec] = co_await conn.run(cfg);
      BOOST_TEST_EQ(ec, canceled_condition());

      co_return {};
   };

   auto result = co_await capy::when_any(exec_fn(), run_fn());
   BOOST_TEST_EQ(result.index(), 1u);  // Exec finished 1st
}

// If no Sentinel is reachable, an error is issued.
// This tests disabling reconnection with Sentinel, too.
capy::task<> test_error_no_sentinel_reachable()
{
   std::string logs;
   co_connection conn{co_await capy::this_coro::executor, make_string_logger(logs)};

   config cfg;
   cfg.sentinel.addresses = {
      {"localhost", "43210"},
      {"localhost", "43211"},
   };
   cfg.sentinel.master_name = "mymaster";
   cfg.reconnect_wait_interval = 0s;  // disable reconnection so we can verify the error

   auto [ec] = co_await conn.run(cfg);
   BOOST_TEST_EQ(ec, error::sentinel_resolve_failed);

   if (
      !BOOST_TEST_NE(
         logs.find("Sentinel at localhost:43210: connection establishment error"),
         std::string::npos) ||
      !BOOST_TEST_NE(
         logs.find("Sentinel at localhost:43211: connection establishment error"),
         std::string::npos)) {
      std::cerr << "Log was:\n" << logs << std::endl;
   }
}

// If Sentinel doesn't know about the configured master,
// the appropriate error is returned
capy::task<> test_error_unknown_master()
{
   std::string logs;
   co_connection conn{co_await capy::this_coro::executor, make_string_logger(logs)};

   config cfg;
   cfg.sentinel.addresses = {
      {"localhost", "26380"},
   };
   cfg.sentinel.master_name = "unknown_master";
   cfg.reconnect_wait_interval = 0s;  // disable reconnection so we can verify the error

   auto [ec] = co_await conn.run(cfg);
   BOOST_TEST_EQ(ec, error::sentinel_resolve_failed);

   if (!BOOST_TEST_NE(
          logs.find("Sentinel at localhost:26380: doesn't know about the configured master"),
          std::string::npos)) {
      std::cerr << "Log was:\n" << logs << std::endl;
   }
}

// The same applies when connecting to replicas, too
capy::task<> test_error_unknown_master_replica()
{
   std::string logs;
   co_connection conn{co_await capy::this_coro::executor, make_string_logger(logs)};

   config cfg;
   cfg.sentinel.addresses = {
      {"localhost", "26380"},
   };
   cfg.sentinel.master_name = "unknown_master";
   cfg.reconnect_wait_interval = 0s;  // disable reconnection so we can verify the error
   cfg.sentinel.server_role = role::replica;

   auto [ec] = co_await conn.run(cfg);
   BOOST_TEST_EQ(ec, error::sentinel_resolve_failed);

   if (!BOOST_TEST_NE(
          logs.find("Sentinel at localhost:26380: doesn't know about the configured master"),
          std::string::npos)) {
      std::cerr << "Log was:\n" << logs << std::endl;
   }
}

}  // namespace

int main()
{
   // Create the required users in the master, replicas and sentinels
   run_coroutine_test(create_user("6379", "redis_user", "redis_pass"));
   run_coroutine_test(create_user("6380", "redis_user", "redis_pass"));
   run_coroutine_test(create_user("6381", "redis_user", "redis_pass"));
   run_coroutine_test(create_user("26379", "sentinel_user", "sentinel_pass"));
   run_coroutine_test(create_user("26380", "sentinel_user", "sentinel_pass"));
   run_coroutine_test(create_user("26381", "sentinel_user", "sentinel_pass"));

   // Actual tests
   run_coroutine_test(test_exec());
   run_coroutine_test(test_receive());
   run_coroutine_test(test_reconnect());
   run_coroutine_test(test_sentinel_not_reachable());
   run_coroutine_test(test_auth());
   run_coroutine_test(test_tls());
   run_coroutine_test(test_replica());

   run_coroutine_test(test_error_no_sentinel_reachable());
   run_coroutine_test(test_error_unknown_master());
   run_coroutine_test(test_error_unknown_master_replica());

   return boost::report_errors();
}
