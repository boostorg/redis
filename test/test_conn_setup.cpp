//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/config.hpp>
#include <boost/redis/connection.hpp>
#include <boost/redis/error.hpp>
#include <boost/redis/logger.hpp>

#include <boost/capy/cond.hpp>
#include <boost/capy/error.hpp>
#include <boost/capy/ex/run_async.hpp>
#include <boost/capy/ex/this_coro.hpp>
#include <boost/capy/io_task.hpp>
#include <boost/capy/task.hpp>
#include <boost/capy/when_any.hpp>
#include <boost/core/lightweight_test.hpp>

#include "common.hpp"

#include <iostream>
#include <string>
#include <string_view>
#include <system_error>

namespace asio = boost::asio;

using namespace boost::redis;
using namespace std::chrono_literals;
namespace capy = boost::capy;
namespace corosio = boost::corosio;

namespace {

capy::task<void> create_user(
   std::string_view port,
   std::string_view username,
   std::string_view password)
{
   connection conn{(co_await capy::this_coro::executor).context()};

   auto exec_fn = [&]() -> capy::task<void> {
      // Enable the user and grant them permissions on everything
      request req;
      req.push("ACL", "SETUSER", username, "on", ">" + std::string(password), "~*", "&*", "+@all");

      auto [ec] = co_await conn.exec(req, ignore);
      BOOST_TEST_EQ(ec, std::error_code());
   };

   auto run_fn = [&]() -> capy::task<void> {
      config cfg;
      cfg.addr.port = port;

      auto [ec] = co_await conn.run(cfg);
      BOOST_TEST_EQ(ec, std::error_code(capy::error::canceled));
   };

   co_await capy::when_any(exec_fn(), run_fn());
}

capy::task<> test_auth_success()
{
   // Setup
   connection conn{(co_await capy::this_coro::executor).context()};

   auto request_fn = [&] -> capy::task<void> {
      // This request should return the username we're logged in as
      request req;
      req.push("ACL", "WHOAMI");
      response<std::string> resp;

      auto [ec] = co_await conn.exec(req, resp);
      BOOST_TEST_EQ(ec, std::error_code());
      BOOST_TEST_EQ(std::get<0>(resp).value(), "myuser");
   };

   auto run_fn = [&] -> capy::task<void> {
      // These credentials are set up in main, before tests are run
      config cfg;
      cfg.username = "myuser";
      cfg.password = "mypass";

      auto [ec] = co_await conn.run(cfg);
      BOOST_TEST_EQ(ec, std::error_code(capy::error::canceled));
   };

   co_await capy::when_any(request_fn(), run_fn());
}

// Verify that we log appropriately (see https://github.com/boostorg/redis/issues/297)
capy::task<> test_auth_failure()
{
   // Setup
   std::string logs;
   connection conn{(co_await capy::this_coro::executor).context(), make_string_logger(logs)};

   // Disable reconnection so the hello error causes the connection to exit
   auto cfg = make_test_config();
   cfg.username = "myuser";
   cfg.password = "wrongpass";  // wrong
   cfg.reconnect_wait_interval = 0s;

   auto [ec] = co_await conn.run(cfg);
   BOOST_TEST_EQ(ec, std::error_code(error::resp3_hello));

   // Check the log
   if (!BOOST_TEST_NE(logs.find("WRONGPASS"), std::string::npos)) {
      std::cerr << "Log was: \n" << logs << std::endl;
   }
}

capy::task<> test_database_index()
{
   // Setup
   connection conn{(co_await capy::this_coro::executor).context()};

   auto request_fn = [&] -> capy::task<void> {
      request req;
      req.push("CLIENT", "INFO");
      response<std::string> resp;

      auto [ec] = co_await conn.exec(req, resp);

      BOOST_TEST_EQ(ec, std::error_code());
      BOOST_TEST_EQ(find_client_info(std::get<0>(resp).value(), "db"), "2");
   };

   auto run_fn = [&] -> capy::task<void> {
      // Use a non-default database index
      auto cfg = make_test_config();
      cfg.database_index = 2;
      auto [ec] = co_await conn.run(cfg);
      BOOST_TEST_EQ(ec, std::error_code(capy::error::canceled));
   };

   co_await capy::when_any(request_fn(), run_fn());
}

// The user configured an empty setup request. No request should be sent
capy::task<> test_setup_empty()
{
   // Setup
   connection conn{(co_await capy::this_coro::executor).context()};

   auto request_fn = [&] -> capy::task<void> {
      request req;
      req.push("CLIENT", "INFO");
      response<std::string> resp;

      auto [ec] = co_await conn.exec(req, resp);

      BOOST_TEST_EQ(ec, std::error_code());
      BOOST_TEST_EQ(find_client_info(std::get<0>(resp).value(), "resp"), "2");  // using RESP2
   };

   auto run_fn = [&] -> capy::task<void> {
      auto cfg = make_test_config();
      cfg.use_setup = true;
      cfg.setup.clear();
      auto [ec] = co_await conn.run(cfg);
      BOOST_TEST_EQ(ec, std::error_code(capy::error::canceled));
   };

   co_await capy::when_any(request_fn(), run_fn());
}

// We can use the setup member to run commands at startup
capy::task<> test_setup_hello()
{
   // Setup
   connection conn{(co_await capy::this_coro::executor).context()};

   auto request_fn = [&] -> capy::task<void> {
      request req;
      req.push("CLIENT", "INFO");
      response<std::string> resp;

      auto [ec] = co_await conn.exec(req, resp);

      BOOST_TEST_EQ(ec, std::error_code());
      BOOST_TEST_EQ(find_client_info(std::get<0>(resp).value(), "resp"), "3");  // using RESP3
      BOOST_TEST_EQ(find_client_info(std::get<0>(resp).value(), "user"), "myuser");
      BOOST_TEST_EQ(find_client_info(std::get<0>(resp).value(), "db"), "8");
   };

   auto run_fn = [&] -> capy::task<void> {
      auto cfg = make_test_config();
      cfg.use_setup = true;
      cfg.setup.clear();
      cfg.setup.push("HELLO", "3", "AUTH", "myuser", "mypass");
      cfg.setup.push("SELECT", 8);
      auto [ec] = co_await conn.run(cfg);
      BOOST_TEST_EQ(ec, std::error_code(capy::error::canceled));
   };

   co_await capy::when_any(request_fn(), run_fn());
}

// Running a pipeline without a HELLO is okay (regression check: we set the priority flag)
capy::task<> test_setup_no_hello()
{
   // Setup
   connection conn{(co_await capy::this_coro::executor).context()};

   auto request_fn = [&] -> capy::task<void> {
      request req;
      req.push("CLIENT", "INFO");
      response<std::string> resp;

      auto [ec] = co_await conn.exec(req, resp);

      BOOST_TEST_EQ(ec, std::error_code());
      BOOST_TEST_EQ(find_client_info(std::get<0>(resp).value(), "resp"), "2");  // using RESP2
      BOOST_TEST_EQ(find_client_info(std::get<0>(resp).value(), "db"), "8");
   };

   auto run_fn = [&] -> capy::task<void> {
      auto cfg = make_test_config();
      cfg.use_setup = true;
      cfg.setup.clear();
      cfg.setup.push("SELECT", 8);
      auto [ec] = co_await conn.run(cfg);
      BOOST_TEST_EQ(ec, std::error_code(capy::error::canceled));
   };

   co_await capy::when_any(request_fn(), run_fn());
}

// Verify that we log appropriately (see https://github.com/boostorg/redis/issues/297)
capy::task<> test_setup_failure()
{
   // Setup
   std::string logs;
   connection conn{(co_await capy::this_coro::executor).context(), make_string_logger(logs)};

   // Disable reconnection so the hello error causes the connection to exit
   auto cfg = make_test_config();
   cfg.use_setup = true;
   cfg.setup.clear();
   cfg.setup.push("GET", "two", "args");  // GET only accepts one arg, so this will fail
   cfg.reconnect_wait_interval = 0s;

   auto [ec] = co_await conn.run(cfg);
   BOOST_TEST_EQ(ec, std::error_code(error::resp3_hello));

   // Check the log
   if (!BOOST_TEST_NE(logs.find("wrong number of arguments"), std::string::npos)) {
      std::cerr << "Log was:\n" << logs << std::endl;
   }
}

}  // namespace

int main()
{
   run_coroutine_test(create_user("6379", "myuser", "mypass"));

   run_coroutine_test(test_auth_success());
   run_coroutine_test(test_auth_failure());
   run_coroutine_test(test_database_index());
   run_coroutine_test(test_setup_empty());
   run_coroutine_test(test_setup_hello());
   run_coroutine_test(test_setup_no_hello());
   run_coroutine_test(test_setup_failure());

   return boost::report_errors();
}
