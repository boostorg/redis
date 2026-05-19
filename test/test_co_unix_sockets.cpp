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
#include <boost/redis/request.hpp>
#include <boost/redis/response.hpp>

#include <boost/capy/ex/this_coro.hpp>
#include <boost/capy/task.hpp>
#include <boost/capy/when_any.hpp>
#include <boost/core/lightweight_test.hpp>

#include "common.hpp"
#include "corosio_common.hpp"

#include <iostream>
#include <string>
#include <string_view>
#include <system_error>

namespace capy = boost::capy;
using namespace boost::redis;
using namespace boost::redis::test;
using error_code = std::error_code;
using namespace std::string_view_literals;

namespace {

constexpr std::string_view unix_socket_path = "/tmp/redis-socks/redis.sock";

// Executing commands using UNIX sockets works
capy::task<void> test_exec()
{
   co_connection conn{co_await capy::this_coro::executor};
   auto cfg = make_test_config();
   cfg.unix_socket = unix_socket_path;

   request req;
   req.push("PING", "unix");
   response<std::string> res;

   auto exec_fn = [&]() -> capy::io_task<> {
      auto [ec] = co_await conn.exec(req, res);
      BOOST_TEST_EQ(ec, error_code());
      co_return {};
   };

   auto run_fn = [&]() -> capy::io_task<> {
      auto [ec] = co_await conn.run(cfg);
      BOOST_TEST_EQ(ec, canceled_condition());
      co_return {};
   };

   co_await capy::when_any(exec_fn(), run_fn());
   BOOST_TEST_EQ(std::get<0>(res).value(), "unix"sv);
}

// If the connection is lost when using a UNIX socket, we can reconnect
capy::task<void> test_reconnection()
{
   co_connection conn{co_await capy::this_coro::executor};
   auto cfg = make_test_config();
   cfg.unix_socket = unix_socket_path;

   request ping_request;
   ping_request.get_config().cancel_if_not_connected = false;
   ping_request.get_config().cancel_if_unresponded = false;
   ping_request.get_config().cancel_on_connection_lost = false;
   ping_request.push("PING", "some_value");

   request quit_request;
   quit_request.push("QUIT");

   auto exec_fn = [&]() -> capy::io_task<> {
      auto [quit_ec] = co_await conn.exec(quit_request, ignore);
      BOOST_TEST_EQ(quit_ec, error_code());

      auto [ping_ec] = co_await conn.exec(ping_request, ignore);
      BOOST_TEST_EQ(ping_ec, error_code());

      co_return {};
   };

   auto run_fn = [&]() -> capy::io_task<> {
      auto [ec] = co_await conn.run(cfg);
      BOOST_TEST_EQ(ec, canceled_condition());
      co_return {};
   };

   co_await capy::when_any(exec_fn(), run_fn());
}

// We can freely switch between UNIX sockets and other transports
capy::task<void> test_switch_between_transports()
{
   co_connection conn{co_await capy::this_coro::executor};
   request req;
   req.push("PING", "hello");

   // Create configurations for TLS and UNIX connections
   auto tcp_tls_cfg = make_test_config();
   tcp_tls_cfg.use_ssl = true;
   tcp_tls_cfg.addr.port = "16380";
   auto unix_cfg = make_test_config();
   unix_cfg.unix_socket = unix_socket_path;

   auto run_once = [&](const config& cfg) -> capy::task<> {
      auto when_any_res = co_await capy::when_any(
         [&]() -> capy::io_task<> {
            response<std::string> res;
            auto [ec] = co_await conn.exec(req, res);
            BOOST_TEST_EQ(ec, error_code());
            BOOST_TEST_EQ(std::get<0>(res).value(), "hello");
            co_return {};
         }(),
         [&]() -> capy::io_task<> {
            auto [ec] = co_await conn.run(cfg);
            BOOST_TEST_EQ(ec, canceled_condition());
            co_return {};
         }());
      BOOST_TEST_EQ(when_any_res.index(), 1);
   };

   // Run the connection with TCP/TLS
   std::cerr << "test_switch_between_transports: TLS 1\n";
   co_await run_once(tcp_tls_cfg);

   // Switch to UNIX
   std::cerr << "test_switch_between_transports: UNIX\n";
   co_await run_once(unix_cfg);

   // Go back to TCP/TLS
   std::cerr << "test_switch_between_transports: TLS 2\n";
   co_await run_once(tcp_tls_cfg);
}

// Trying to enable TLS and UNIX sockets at the same time
// is an error and makes run exit immediately
capy::task<void> test_error_unix_tls()
{
   co_connection conn{co_await capy::this_coro::executor};
   auto cfg = make_test_config();
   cfg.use_ssl = true;
   cfg.addr.port = "16380";
   cfg.unix_socket = unix_socket_path;

   auto [ec] = co_await conn.run(cfg);
   BOOST_TEST_EQ(ec, error::unix_sockets_ssl_unsupported);
}

}  // namespace

int main()
{
   run_coroutine_test(test_exec());
   run_coroutine_test(test_reconnection());
   run_coroutine_test(test_switch_between_transports());
   run_coroutine_test(test_error_unix_tls());

   return boost::report_errors();
}
