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

#include <boost/capy/cond.hpp>
#include <boost/capy/ex/immediate.hpp>
#include <boost/capy/ex/this_coro.hpp>
#include <boost/capy/io_task.hpp>
#include <boost/capy/task.hpp>
#include <boost/capy/timeout.hpp>
#include <boost/capy/when_any.hpp>
#include <boost/core/lightweight_test.hpp>

#include "common.hpp"
#include "corosio_common.hpp"

#include <chrono>
#include <memory>
#include <string>
#include <system_error>

namespace capy = boost::capy;
using namespace boost::redis;
using namespace boost::redis::test;
using namespace std::chrono_literals;
using error_code = std::error_code;

namespace {

// We can cancel requests that haven't been written yet
capy::task<> test_cancel_pending()
{
   co_connection conn{co_await capy::this_coro::executor};

   // Issue a request without calling run, so the request stays waiting forever
   auto exec_fn = [&]() -> capy::io_task<> {
      request req;
      req.push("get", "mykey");
      auto [ec] = co_await conn.exec(req, ignore);
      BOOST_TEST_EQ(ec, canceled_condition());
      co_return {};
   };

   auto result = co_await capy::when_any(exec_fn(), capy::ready());
   BOOST_TEST_EQ(result.index(), 2u);  // Trigger finished 1st
}

// We can cancel requests that have been written but whose
// responses haven't been received yet.
capy::task<> test_cancel_written()
{
   co_connection conn{co_await capy::this_coro::executor};

   auto exec_fn = [&]() -> capy::io_task<> {
      // Will be cancelled after it has been written but before the response arrives.
      // Our BLPOP will block server-side for longer than the deadline below.
      auto req1 = std::make_unique<request>();
      req1->push("BLPOP", "any", 1);
      auto resp1 = std::make_unique<response<std::string>>();
      auto [ec1] = co_await capy::timeout(conn.exec(*req1, *resp1), 500ms);
      BOOST_TEST_EQ(ec1, condition_wrapper{capy::cond::timeout});

      // Destroy request and response to verify that we don't reference them after cancellation
      req1.reset();
      resp1.reset();

      // The connection remains usable. The PING's response will be received
      // after the BLPOP's response, but it will be processed successfully.
      request req2;
      req2.push("PING", "after_blpop");
      response<std::string> resp2;
      auto [ec2] = co_await conn.exec(req2, resp2);
      BOOST_TEST_EQ(ec2, error_code());
      BOOST_TEST_EQ(std::get<0>(resp2).value(), "after_blpop");
      co_return {};
   };

   auto run_fn = [&]() -> capy::io_task<> {
      auto cfg = make_test_config();
      cfg.health_check_interval = 0s;

      auto [ec] = co_await conn.run(cfg);
      BOOST_TEST_EQ(ec, canceled_condition());
      co_return {};
   };

   auto result = co_await capy::when_any(exec_fn(), run_fn());
   BOOST_TEST_EQ(result.index(), 1u);  // Exec finished 1st
}

}  // namespace

int main()
{
   run_coroutine_test(test_cancel_pending());
   run_coroutine_test(test_cancel_written());

   return boost::report_errors();
}
