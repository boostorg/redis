//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/detail/flow_controller.hpp>

#include <boost/capy/continuation.hpp>
#include <boost/capy/ex/immediate.hpp>
#include <boost/capy/io_task.hpp>
#include <boost/capy/task.hpp>
#include <boost/capy/when_all.hpp>
#include <boost/capy/when_any.hpp>
#include <boost/core/lightweight_test.hpp>

#include "corosio_common.hpp"

#include <coroutine>
#include <memory>
#include <system_error>

using namespace boost::redis;
using namespace boost::redis::test;
namespace capy = boost::capy;
using detail::flow_controller;

namespace {

// Yields twice so any pending handlers are dispatched.
// This is a better alternative than waiting for small periods of time with delay()
capy::task<> yield()
{
   capy::continuation cont{};

   struct yield_awaitable {
      capy::continuation* cont;

      constexpr bool await_ready() const noexcept { return false; }

      std::coroutine_handle<> await_suspend(std::coroutine_handle<> h, capy::io_env const* env)
      {
         cont->h = h;
         env->executor.post(*cont);
         return std::noop_coroutine();
      }

      void await_resume() { }
   } aw{&cont};

   co_await aw;
   co_await aw;
}

// If take() is called in the initial state, it waits (regression check)
capy::task<> test_take_initial()
{
   flow_controller cont{64u};
   bool point1_reached = false;

   auto [ec, a, b] = co_await capy::when_all(
      [&]() -> capy::io_task<> {
         // Initially, there are no bytes to be taken, so this blocks
         auto [ec] = co_await cont.take();
         BOOST_TEST_EQ(ec, std::error_code());

         // Signal that take has returned
         point1_reached = true;
         co_return {};
      }(),
      [&]() -> capy::io_task<> {
         // Verify that take() didn't complete immediately
         co_await yield();
         BOOST_TEST_NOT(point1_reached);

         // Unblock take()
         BOOST_TEST(cont.try_put(20u));
         co_return {};
      }());

   BOOST_TEST_EQ(ec, std::error_code());
}

// If take() is called and there are

}  // namespace

int main()
{
   run_coroutine_test(test_take_initial());

   return boost::report_errors();
}
