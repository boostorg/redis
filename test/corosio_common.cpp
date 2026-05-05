//
// Copyright (c) 2026 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/co_connection.hpp>

#include <boost/assert/source_location.hpp>
#include <boost/capy/cond.hpp>
#include <boost/capy/error.hpp>
#include <boost/capy/ex/run_async.hpp>
#include <boost/capy/when_any.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/corosio/io_context.hpp>

#include "common.hpp"
#include "corosio_common.hpp"

#include <iostream>

namespace capy = boost::capy;

void boost::redis::test::run_coroutine_test(capy::task<void> test, source_location loc)
{
   // Set a timeout to the tests, so they don't hang on error
   bool finished = false;
   auto wrapper_fn = [test = std::move(test), &finished]() mutable -> capy::task<void> {
      co_await std::move(test);
      finished = true;
   };

   // Actually run the test
   corosio::io_context ctx;
   capy::run_async(ctx.get_executor())(wrapper_fn());
   ctx.run_for(test_timeout);

   // Check that it finished
   if (!BOOST_TEST(finished))
      std::cerr << "  Called from " << loc << std::endl;
}

capy::task<void> boost::redis::test::create_user(
   std::string_view port,
   std::string_view username,
   std::string_view password,
   source_location loc)
{
   co_connection conn{co_await capy::this_coro::executor};

   auto exec_fn = [&]() -> capy::io_task<> {
      // Enable the user and grant them permissions on everything
      request req;
      req.push("ACL", "SETUSER", username, "on", ">" + std::string(password), "~*", "&*", "+@all");

      auto [ec] = co_await conn.exec(req, ignore);
      if (!BOOST_TEST_EQ(ec, std::error_code()))
         std::cerr << "  Called from " << loc << std::endl;

      co_return {};
   };

   auto run_fn = [&]() -> capy::io_task<> {
      config cfg;
      cfg.addr.port = port;

      auto [ec] = co_await conn.run(cfg);
      if (!BOOST_TEST_EQ(ec, canceled_condition()))
         std::cerr << "  Called from " << loc << std::endl;

      co_return {};
   };

   auto result = co_await capy::when_any(exec_fn(), run_fn());
   if (!BOOST_TEST_EQ(result.index(), 1u))  // Exec finished 1st
      std::cerr << "  Called from " << loc << std::endl;
}

condition_wrapper boost::redis::test::capy_canceled_condition() { return {capy::cond::canceled}; }
