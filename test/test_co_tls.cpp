//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/co_connection.hpp>
#include <boost/redis/ignore.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/response.hpp>

#include <boost/capy/ex/this_coro.hpp>
#include <boost/capy/io_task.hpp>
#include <boost/capy/task.hpp>
#include <boost/capy/when_any.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/corosio/tls_context.hpp>
#include <boost/system/system_error.hpp>

#include "common.hpp"
#include "corosio_common.hpp"

#include <cerrno>
#include <fstream>
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

// Loads the CA certificate that signed the certificate used by the server.
std::string load_ca_certificate()
{
   auto ca_path = safe_getenv("BOOST_REDIS_CA_PATH", "/opt/ci-tls/ca.crt");
   std::ifstream f(ca_path);
   if (!f) {
      throw boost::system::system_error(
         errno,
         boost::system::system_category(),
         "Failed to open CA certificate file '" + ca_path + "'");
   }

   return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

config make_tls_config()
{
   config cfg;
   cfg.use_ssl = true;
   cfg.addr.host = get_server_hostname();
   cfg.addr.port = "16379";
   return cfg;
}

// Using the default TLS context (the one created if nothing is passed to the ctor)
// allows establishing TLS connections and execute requests
capy::task<> test_exec_default_tls_context()
{
   co_connection conn{co_await capy::this_coro::executor};

   auto exec_fn = [&]() -> capy::io_task<> {
      request req;
      req.push("PING", "test_co_tls");
      response<std::string> resp;

      auto [ec] = co_await conn.exec(req, resp);
      BOOST_TEST_EQ(ec, error_code());
      BOOST_TEST_EQ(std::get<0>(resp).value(), "test_co_tls");

      co_return {};
   };

   auto run_fn = [&]() -> capy::io_task<> {
      auto [ec] = co_await conn.run(make_tls_config());
      BOOST_TEST_EQ(ec, canceled_condition());
      co_return {};
   };

   auto result = co_await capy::when_any(exec_fn(), run_fn());
   BOOST_TEST_EQ(result.index(), 1u);  // Exec finished 1st
}

// Users can pass a custom context with TLS config
capy::task<> test_exec_custom_ssl_context()
{
   // Configure the TLS context to trust the CA that signed the server's certificate.
   // The test certificate uses "redis" as its common name,
   // regardless of the actual server's hostname.
   corosio::tls_context tls_ctx;
   auto ec_ca = tls_ctx.add_certificate_authority(load_ca_certificate());
   BOOST_TEST_EQ(ec_ca, error_code());
   auto ec_mode = tls_ctx.set_verify_mode(corosio::tls_verify_mode::require_peer);
   BOOST_TEST_EQ(ec_mode, error_code());
   tls_ctx.set_hostname("redis");

   co_connection conn{co_await capy::this_coro::executor, std::move(tls_ctx)};

   auto exec_fn = [&]() -> capy::io_task<> {
      constexpr std::string_view ping_value = "Kabuf";
      request req;
      req.push("PING", ping_value);
      response<std::string> resp;

      auto [ec] = co_await conn.exec(req, resp);
      BOOST_TEST_EQ(ec, error_code());
      BOOST_TEST_EQ(std::get<0>(resp).value(), ping_value);

      co_return {};
   };

   auto run_fn = [&]() -> capy::io_task<> {
      auto [ec] = co_await conn.run(make_tls_config());
      BOOST_TEST_EQ(ec, canceled_condition());
      co_return {};
   };

   auto result = co_await capy::when_any(exec_fn(), run_fn());
   BOOST_TEST_EQ(result.index(), 1u);  // Exec finished 1st
}

// After an error, a connection can recover.
// Force an error using QUIT, then issue a regular request to verify that we could reconnect.
capy::task<> test_reconnection()
{
   co_connection conn{co_await capy::this_coro::executor};

   auto exec_fn = [&]() -> capy::io_task<> {
      request quit_request;
      quit_request.push("QUIT");
      auto [ec_quit] = co_await conn.exec(quit_request, ignore);
      BOOST_TEST_EQ(ec_quit, error_code());

      request ping_request;
      ping_request.push("PING", "some_value");
      ping_request.get_config().cancel_if_unresponded = false;
      ping_request.get_config().cancel_on_connection_lost = false;
      auto [ec_ping] = co_await conn.exec(ping_request, ignore);
      BOOST_TEST_EQ(ec_ping, error_code());

      co_return {};
   };

   auto run_fn = [&]() -> capy::io_task<> {
      auto [ec] = co_await conn.run(make_test_config());
      BOOST_TEST_EQ(ec, canceled_condition());
      co_return {};
   };

   auto result = co_await capy::when_any(exec_fn(), run_fn());
   BOOST_TEST_EQ(result.index(), 1u);  // Exec finished 1st
}

}  // namespace

int main()
{
   run_coroutine_test(test_exec_default_tls_context());
   run_coroutine_test(test_exec_custom_ssl_context());
   run_coroutine_test(test_reconnection());

   return boost::report_errors();
}
