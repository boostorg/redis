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
#include <boost/redis/logger.hpp>

#include <boost/capy/ex/this_coro.hpp>
#include <boost/capy/task.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/corosio/tls_context.hpp>
#include <boost/system/error_code.hpp>

#include "corosio_common.hpp"

#include <string>
#include <string_view>
#include <vector>

using boost::system::error_code;
namespace capy = boost::capy;
namespace corosio = boost::corosio;
using namespace boost::redis;
using namespace boost::redis::test;

namespace {

config make_invalid_config()
{
   config cfg;
   cfg.use_ssl = true;
   cfg.unix_socket = "/tmp/sock";
   return cfg;
}

struct fixture {
   std::vector<std::string> messages;
   logger make_logger(logger::level lvl)
   {
      return {lvl, [this](logger::level, std::string_view msg) {
                 messages.emplace_back(msg);
              }};
   }
};

capy::task<> test_connection_constructor_executor_1()
{
   // Setup
   fixture fix;
   co_connection conn{co_await capy::this_coro::executor, fix.make_logger(logger::level::info)};

   // Produce some logging
   auto [ec] = co_await conn.run(make_invalid_config());
   BOOST_TEST_EQ(ec, error::unix_sockets_ssl_unsupported);

   // Some logging was produced
   BOOST_TEST_EQ(fix.messages.size(), 1u);
}

capy::task<> test_connection_constructor_context_1()
{
   // Setup
   fixture fix;
   auto ex = co_await capy::this_coro::executor;
   co_connection conn{ex.context(), fix.make_logger(logger::level::info)};

   // Produce some logging
   auto [ec] = co_await conn.run(make_invalid_config());
   BOOST_TEST_EQ(ec, error::unix_sockets_ssl_unsupported);

   // Some logging was produced
   BOOST_TEST_EQ(fix.messages.size(), 1u);
}

capy::task<> test_connection_constructor_executor_2()
{
   // Setup
   fixture fix;
   co_connection conn{
      co_await capy::this_coro::executor,
      corosio::tls_context{},
      fix.make_logger(logger::level::info)};

   // Produce some logging
   auto [ec] = co_await conn.run(make_invalid_config());
   BOOST_TEST_EQ(ec, error::unix_sockets_ssl_unsupported);

   // Some logging was produced
   BOOST_TEST_EQ(fix.messages.size(), 1u);
}

capy::task<> test_connection_constructor_context_2()
{
   // Setup
   fixture fix;
   auto ex = co_await capy::this_coro::executor;
   co_connection conn{ex.context(), corosio::tls_context{}, fix.make_logger(logger::level::info)};

   // Produce some logging
   auto [ec] = co_await conn.run(make_invalid_config());
   BOOST_TEST_EQ(ec, error::unix_sockets_ssl_unsupported);

   // Some logging was produced
   BOOST_TEST_EQ(fix.messages.size(), 1u);
}

capy::task<> test_disable_logging()
{
   // Setup
   fixture fix;
   co_connection conn{co_await capy::this_coro::executor, fix.make_logger(logger::level::disabled)};

   // Produce some logging
   auto [ec] = co_await conn.run(make_invalid_config());
   BOOST_TEST_EQ(ec, error::unix_sockets_ssl_unsupported);

   // No logging should have been produced
   BOOST_TEST_EQ(fix.messages.size(), 0u);
}

}  // namespace

int main()
{
   run_coroutine_test(test_connection_constructor_executor_1());
   run_coroutine_test(test_connection_constructor_executor_2());
   run_coroutine_test(test_connection_constructor_context_1());
   run_coroutine_test(test_connection_constructor_context_2());
   run_coroutine_test(test_disable_logging());

   return boost::report_errors();
}
