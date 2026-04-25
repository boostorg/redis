//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/co_connection.hpp>
#include <boost/redis/config.hpp>

#include <boost/capy/ex/immediate.hpp>
#include <boost/capy/when_any.hpp>
#include <boost/core/lightweight_test.hpp>

#include "common.hpp"
#include "corosio_common.hpp"

using boost::system::error_code;
namespace capy = boost::capy;
using namespace boost::redis;
using namespace boost::redis::test;

namespace {

// The user configured an empty setup request. No request should be sent
capy::task<> test_cancel_run()
{
   // Setup
   co_connection conn{co_await capy::this_coro::executor};

   auto run_fn = [&]() -> capy::io_task<> {
      auto [ec] = co_await conn.run(config{});
      BOOST_TEST_EQ(ec, canceled_condition());
      co_return {};
   };

   auto result = co_await capy::when_any(run_fn(), capy::ready());
   BOOST_TEST_EQ(result.index(), 2u);  // Ready finished 1st
}

}  // namespace

int main()
{
   run_coroutine_test(test_cancel_run());

   return boost::report_errors();
}
