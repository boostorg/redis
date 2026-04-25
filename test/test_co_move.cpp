//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/co_connection.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/response.hpp>

#include <boost/capy/delay.hpp>
#include <boost/capy/ex/this_coro.hpp>
#include <boost/capy/io_task.hpp>
#include <boost/capy/task.hpp>
#include <boost/capy/when_any.hpp>
#include <boost/core/lightweight_test.hpp>

#include "common.hpp"
#include "corosio_common.hpp"

#include <string>
#include <system_error>
#include <utility>

namespace capy = boost::capy;
using namespace boost::redis;
using namespace boost::redis::test;
using namespace std::chrono_literals;
using error_code = std::error_code;

namespace {

// Move constructing a connection doesn't leave dangling pointers
capy::task<> test_conn_move_construct()
{
   co_connection conn_prev{co_await capy::this_coro::executor};
   co_connection conn{std::move(conn_prev)};

   response<std::string> resp;

   auto exec_fn = [&]() -> capy::io_task<> {
      request req;
      req.push("PING", "something");
      auto [ec] = co_await conn.exec(req, resp);
      BOOST_TEST_EQ(ec, error_code());
      co_return {};
   };

   auto run_fn = [&]() -> capy::io_task<> {
      auto [ec] = co_await conn.run(make_test_config());
      BOOST_TEST_EQ(ec, canceled_condition());
      co_return {};
   };

   auto result = co_await capy::when_any(exec_fn(), run_fn());
   BOOST_TEST_EQ(result.index(), 1u);  // Exec finished 1st
   BOOST_TEST_EQ(std::get<0>(resp).value(), "something");
}

// Moving a connection is safe even when it's running,
// and it doesn't leave dangling pointers
capy::task<> test_conn_move_assign_while_running()
{
   co_connection conn{co_await capy::this_coro::executor};
   co_connection conn2{co_await capy::this_coro::executor};  // will be assigned to

   response<std::string> resp;

   auto exec_fn = [&]() -> capy::io_task<> {
      // Wait briefly to ensure run is in flight
      auto [delay_ec] = co_await capy::delay(50ms);
      BOOST_TEST_EQ(delay_ec, error_code());

      // Perform the move while run is in progress
      conn2 = std::move(conn);

      // Launch a PING on the moved-to connection
      request req;
      req.push("PING", "something");
      auto [ec] = co_await conn2.exec(req, resp);
      BOOST_TEST_EQ(ec, error_code());
      co_return {};
   };

   auto run_fn = [&]() -> capy::io_task<> {
      auto [ec] = co_await conn.run(make_test_config());
      BOOST_TEST_EQ(ec, canceled_condition());
      co_return {};
   };

   auto result = co_await capy::when_any(exec_fn(), run_fn());
   BOOST_TEST_EQ(result.index(), 1u);  // Exec finished 1st
   BOOST_TEST_EQ(std::get<0>(resp).value(), "something");
}

}  // namespace

int main()
{
   run_coroutine_test(test_conn_move_construct());
   run_coroutine_test(test_conn_move_assign_while_running());

   return boost::report_errors();
}
