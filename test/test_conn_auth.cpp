//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/connection.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/response.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/system/error_code.hpp>

#include "common.hpp"

namespace asio = boost::asio;
namespace redis = boost::redis;
using namespace std::chrono_literals;
using boost::system::error_code;

namespace {

// Creates a user with a known password. Harmless if the user already exists
void setup_password()
{
   // Setup
   asio::io_context ioc;
   redis::connection conn{ioc};

   // Enable the user and grant them permissions on everything
   redis::request req;
   req.push("ACL", "SETUSER", "myuser", "on", ">mypass", "~*", "&*", "+@all");
   redis::generic_response resp;

   bool run_finished = false, exec_finished = false;
   conn.async_run(make_test_config(), [&](error_code ec) {
      run_finished = true;
      BOOST_TEST_EQ(ec, asio::error::operation_aborted);
   });

   conn.async_exec(req, resp, [&](error_code ec, std::size_t) {
      exec_finished = true;
      BOOST_TEST_EQ(ec, error_code());
      conn.cancel();
   });

   ioc.run_for(test_timeout);

   BOOST_TEST(run_finished);
   BOOST_TEST(exec_finished);
   BOOST_TEST(resp.has_value());
}

void test_auth_success()
{
   // Setup
   asio::io_context ioc;
   redis::connection conn{ioc};

   // This request should return the username we're logged in as
   redis::request req;
   req.push("ACL", "WHOAMI");
   redis::response<std::string> resp;

   // These credentials are set up in main, before tests are run
   auto cfg = make_test_config();
   cfg.username = "myuser";
   cfg.password = "mypass";

   bool exec_finished = false, run_finished = false;

   conn.async_exec(req, resp, [&](error_code ec, std::size_t) {
      exec_finished = true;
      BOOST_TEST_EQ(ec, error_code());
      conn.cancel();
   });

   conn.async_run(cfg, {}, [&](error_code ec) {
      run_finished = true;
      BOOST_TEST_EQ(ec, asio::error::operation_aborted);
   });

   ioc.run_for(test_timeout);

   BOOST_TEST(exec_finished);
   BOOST_TEST(run_finished);
   BOOST_TEST_EQ(std::get<0>(resp).value(), "myuser");
}

}  // namespace

int main()
{
   setup_password();
   test_auth_success();

   return boost::report_errors();
}
