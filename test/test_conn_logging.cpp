//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/connection.hpp>

#include <boost/core/lightweight_test.hpp>

#include "boost/asio/io_context.hpp"
#include "boost/asio/ssl/context.hpp"
#include "boost/redis/config.hpp"
#include "boost/redis/logger.hpp"
#include "boost/system/detail/error_code.hpp"
#include "common.hpp"

#include <string>
#include <string_view>
#include <vector>

using boost::system::error_code;
namespace net = boost::asio;
using namespace boost::redis;

namespace {

// user tests
//     logging is enabled by default
//     a custom logger can be used
//     logging can be disabled
//     logging can be changed verbosity
// connection logging tests
//     basic_connection async_run with logger
//     connection constructor 1
//     connection constructor 2
//     connection constructor context 1
//     connection constructor context 2
//     connection async_run with logger keeps working
//     connection async_run with logger and prefix keeps working

template <class Conn>
void run_with_invalid_config(net::io_context& ioc, Conn& conn)
{
   config cfg;
   cfg.use_ssl = true;
   cfg.unix_socket = "/tmp/sock";
   conn.async_run(cfg, [](error_code ec) {
      BOOST_TEST_NE(ec, error_code());
   });
   ioc.run_for(test_timeout);
}

void test_basic_connection_constructor_executor_1()
{
   // Setup
   net::io_context ioc;
   std::vector<std::string> messages;
   logger lgr(logger::level::info, [&](logger::level, std::string_view msg) {
      messages.emplace_back(msg);
   });
   basic_connection<net::io_context::executor_type> conn{ioc.get_executor(), std::move(lgr)};

   // Produce some logging
   run_with_invalid_config(ioc, conn);

   // Some logging was produced
   BOOST_TEST_EQ(messages.size(), 1u);
}

void test_basic_connection_constructor_context_1()
{
   // Setup
   net::io_context ioc;
   std::vector<std::string> messages;
   logger lgr(logger::level::info, [&](logger::level, std::string_view msg) {
      messages.emplace_back(msg);
   });
   basic_connection<net::io_context::executor_type> conn{ioc, std::move(lgr)};

   // Produce some logging
   run_with_invalid_config(ioc, conn);

   // Some logging was produced
   BOOST_TEST_EQ(messages.size(), 1u);
}

void test_basic_connection_constructor_executor_2()
{
   // Setup
   net::io_context ioc;
   std::vector<std::string> messages;
   logger lgr(logger::level::info, [&](logger::level, std::string_view msg) {
      messages.emplace_back(msg);
   });
   basic_connection<net::io_context::executor_type> conn{
      ioc.get_executor(),
      net::ssl::context{net::ssl::context::tlsv12_client},
      std::move(lgr)};

   // Produce some logging
   run_with_invalid_config(ioc, conn);

   // Some logging was produced
   BOOST_TEST_EQ(messages.size(), 1u);
}

void test_basic_connection_constructor_context_2()
{
   // Setup
   net::io_context ioc;
   std::vector<std::string> messages;
   logger lgr(logger::level::info, [&](logger::level, std::string_view msg) {
      messages.emplace_back(msg);
   });
   basic_connection<net::io_context::executor_type> conn{
      ioc,
      net::ssl::context{net::ssl::context::tlsv12_client},
      std::move(lgr)};

   // Produce some logging
   run_with_invalid_config(ioc, conn);

   // Some logging was produced
   BOOST_TEST_EQ(messages.size(), 1u);
}

void test_connection_constructor_executor_1()
{
   // Setup
   net::io_context ioc;
   std::vector<std::string> messages;
   logger lgr(logger::level::info, [&](logger::level, std::string_view msg) {
      messages.emplace_back(msg);
   });
   connection conn{ioc.get_executor(), std::move(lgr)};

   // Produce some logging
   run_with_invalid_config(ioc, conn);

   // Some logging was produced
   BOOST_TEST_EQ(messages.size(), 1u);
}

void test_connection_constructor_context_1()
{
   // Setup
   net::io_context ioc;
   std::vector<std::string> messages;
   logger lgr(logger::level::info, [&](logger::level, std::string_view msg) {
      messages.emplace_back(msg);
   });
   connection conn{ioc, std::move(lgr)};

   // Produce some logging
   run_with_invalid_config(ioc, conn);

   // Some logging was produced
   BOOST_TEST_EQ(messages.size(), 1u);
}

void test_connection_constructor_executor_2()
{
   // Setup
   net::io_context ioc;
   std::vector<std::string> messages;
   logger lgr(logger::level::info, [&](logger::level, std::string_view msg) {
      messages.emplace_back(msg);
   });
   connection conn{
      ioc.get_executor(),
      net::ssl::context{net::ssl::context::tlsv12_client},
      std::move(lgr)};

   // Produce some logging
   run_with_invalid_config(ioc, conn);

   // Some logging was produced
   BOOST_TEST_EQ(messages.size(), 1u);
}

void test_connection_constructor_context_2()
{
   // Setup
   net::io_context ioc;
   std::vector<std::string> messages;
   logger lgr(logger::level::info, [&](logger::level, std::string_view msg) {
      messages.emplace_back(msg);
   });
   connection conn{ioc, net::ssl::context{net::ssl::context::tlsv12_client}, std::move(lgr)};

   // Produce some logging
   run_with_invalid_config(ioc, conn);

   // Some logging was produced
   BOOST_TEST_EQ(messages.size(), 1u);
}

}  // namespace

int main()
{
   test_basic_connection_constructor_executor_1();
   test_basic_connection_constructor_executor_2();
   test_basic_connection_constructor_context_1();
   test_basic_connection_constructor_context_2();

   test_connection_constructor_executor_1();
   test_connection_constructor_executor_2();
   test_connection_constructor_context_1();
   test_connection_constructor_context_2();

   return boost::report_errors();
}
