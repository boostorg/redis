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

   BOOST_TEST_EQ(cont.pending_bytes(), 0u);  // initially, the object is empty

   auto [ec, a, b] = co_await capy::when_all(
      [&]() -> capy::io_task<> {
         // Initially, there are no bytes to be taken, so this blocks
         auto [ec] = co_await cont.take();
         BOOST_TEST_EQ(ec, std::error_code());
         BOOST_TEST_EQ(cont.pending_bytes(), 0u);  // the object was emptied

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

// If take() is called and there are pending bytes, they get consumed.
capy::task<> test_take_pending_bytes()
{
   flow_controller cont{64u};
   bool point1_reached = false, point2_reached = false;

   // Place some bytes in the object
   BOOST_TEST(cont.try_put(20u));
   BOOST_TEST_EQ(cont.pending_bytes(), 20u);

   auto [ec, a, b] = co_await capy::when_all(
      [&]() -> capy::io_task<> {
         // There are pending bytes, so this does not block
         auto [ec] = co_await cont.take();
         BOOST_TEST_EQ(ec, std::error_code());
         BOOST_TEST_EQ(cont.pending_bytes(), 0u);
         point1_reached = true;

         // Subsequent calls would block
         auto [ec2] = co_await cont.take();
         BOOST_TEST_EQ(ec2, std::error_code());
         BOOST_TEST_EQ(cont.pending_bytes(), 0u);
         point2_reached = true;

         co_return {};
      }(),
      [&]() -> capy::io_task<> {
         // Verify that take() completed immediately and the 2nd one didn't
         co_await yield();
         BOOST_TEST(point1_reached);

         co_await yield();
         BOOST_TEST_NOT(point2_reached);

         // Unblock take()
         BOOST_TEST(cont.try_put(20));

         co_return {};
      }());

   BOOST_TEST_EQ(ec, std::error_code());
}

// take() can be cancelled
capy::task<> test_take_cancel()
{
   flow_controller cont{64u};

   auto result = co_await capy::when_any(
      [&]() -> capy::io_task<> {
         auto [ec] = co_await cont.take();
         BOOST_TEST_EQ(ec, capy_canceled_condition());
         co_return {};
      }(),
      capy::ready());

   BOOST_TEST_EQ(result.index(), 2u);  // ready finished 1st
}

// try_put() returns true if the object is not full
void test_try_put_not_full()
{
   flow_controller cont{64u};
   BOOST_TEST(cont.try_put(20u));
   BOOST_TEST_EQ(cont.pending_bytes(), 20u);
   BOOST_TEST(cont.try_put(41u));
   BOOST_TEST_EQ(cont.pending_bytes(), 61u);
   BOOST_TEST(cont.try_put(3u));
   BOOST_TEST_EQ(cont.pending_bytes(), 64u);
}

// try_put() returns false if the object is just full
void test_try_put_just_full()
{
   flow_controller cont{64u};
   BOOST_TEST(cont.try_put(64u));
   BOOST_TEST_EQ(cont.pending_bytes(), 64u);
   BOOST_TEST_NOT(cont.try_put(1u));
   BOOST_TEST_EQ(cont.pending_bytes(), 64u);
}

// try_put() returns true if we fill past the max pending bytes, but the object is then considered full
void test_try_put_past_full()
{
   flow_controller cont{64u};
   BOOST_TEST(cont.try_put(100u));
   BOOST_TEST_EQ(cont.pending_bytes(), 100u);
   BOOST_TEST_NOT(cont.try_put(1u));
   BOOST_TEST_EQ(cont.pending_bytes(), 100u);
}

// try_put() (and in consequence, put()) obey the max_bytes constructor arg
void test_try_put_ctor_arg()
{
   flow_controller cont{100u};
   BOOST_TEST(cont.try_put(80u));
   BOOST_TEST(cont.try_put(19u));
   BOOST_TEST(cont.try_put(2u));
   BOOST_TEST_NOT(cont.try_put(1u));
   BOOST_TEST_EQ(cont.pending_bytes(), 101u);
}

// put() completes immediately in the initial state (no pending bytes)
capy::task<> test_put_initial()
{
   flow_controller cont{64u};
   bool point1_reached = false;

   auto [ec, a, b] = co_await capy::when_all(
      [&]() -> capy::io_task<> {
         // There are no pending bytes, so this does not block
         auto [ec] = co_await cont.put(21u);
         BOOST_TEST_EQ(ec, std::error_code());
         BOOST_TEST_EQ(cont.pending_bytes(), 21u);
         point1_reached = true;
         co_return {};
      }(),
      [&]() -> capy::io_task<> {
         // Verify that put() completed immediately
         co_await yield();
         BOOST_TEST(point1_reached);
         co_return {};
      }());

   BOOST_TEST_EQ(ec, std::error_code());
}

// put() completes immediately if the object is not full
capy::task<> test_put_not_full()
{
   flow_controller cont{64u};
   bool point1_reached = false;

   BOOST_TEST(cont.try_put(21u));

   auto [ec, a, b] = co_await capy::when_all(
      [&]() -> capy::io_task<> {
         // There are no pending bytes, so this does not block
         auto [ec] = co_await cont.put(18u);
         BOOST_TEST_EQ(ec, std::error_code());
         BOOST_TEST_EQ(cont.pending_bytes(), 39u);
         point1_reached = true;
         co_return {};
      }(),
      [&]() -> capy::io_task<> {
         // Verify that put() completed immediately
         co_await yield();
         BOOST_TEST(point1_reached);
         co_return {};
      }());

   BOOST_TEST_EQ(ec, std::error_code());
}

// put() blocks until a take() happens if the object is full
capy::task<> test_put_full()
{
   flow_controller cont{64u};
   bool point1_reached = false;

   BOOST_TEST(cont.try_put(80u));  // fill the object

   auto [ec, a, b] = co_await capy::when_all(
      [&]() -> capy::io_task<> {
         // The object is full, so this blocks
         auto [ec] = co_await cont.put(18u);
         BOOST_TEST_EQ(ec, std::error_code());
         BOOST_TEST_EQ(cont.pending_bytes(), 18u);
         point1_reached = true;
         co_return {};
      }(),
      [&]() -> capy::io_task<> {
         // Verify that put() blocked
         co_await yield();
         BOOST_TEST_NOT(point1_reached);

         // Unblock put
         auto [ec] = co_await cont.take();
         BOOST_TEST_EQ(ec, std::error_code());
         co_return {};
      }());

   BOOST_TEST_EQ(ec, std::error_code());
}

// put() unblocks take()
capy::task<> test_put_unblocks_take()
{
   flow_controller cont{64u};
   bool point1_reached = false;

   auto [ec, a, b] = co_await capy::when_all(
      [&]() -> capy::io_task<> {
         // The object is empty, so this blocks
         auto [ec] = co_await cont.take();
         BOOST_TEST_EQ(ec, std::error_code());
         BOOST_TEST_EQ(cont.pending_bytes(), 0u);
         point1_reached = true;
         co_return {};
      }(),
      [&]() -> capy::io_task<> {
         // Verify that take() blocked
         co_await yield();
         BOOST_TEST_NOT(point1_reached);

         // Calling put() unblocks take()
         auto [ec] = co_await cont.put(10u);
         BOOST_TEST_EQ(ec, std::error_code());
         co_return {};
      }());

   BOOST_TEST_EQ(ec, std::error_code());
}

// put() can be cancelled
capy::task<> test_put_cancel()
{
   flow_controller cont{64u};

   BOOST_TEST(cont.try_put(80u));  // fill the object

   auto result = co_await capy::when_any(
      [&]() -> capy::io_task<> {
         auto [ec] = co_await cont.put(10u);  // will block until cancelled
         BOOST_TEST_EQ(ec, capy_canceled_condition());
         co_return {};
      }(),
      capy::ready());

   BOOST_TEST_EQ(result.index(), 2u);  // ready finished 1st
}

// Full cycle
capy::task<> test_full_cycle()
{
   flow_controller cont{64u};
   int receiver_point = 0, reader_point = 0;

   auto [ec, a, b] = co_await capy::when_all(
      [&]() -> capy::io_task<> {
         // This is the receiver. Initially, no push has arrived and it blocks
         auto [ec] = co_await cont.take();
         BOOST_TEST_EQ(ec, std::error_code());
         BOOST_TEST_EQ(cont.pending_bytes(), 0u);
         receiver_point = 1;

         // The pushes have been consumed. Block for more
         auto [ec2] = co_await cont.take();
         BOOST_TEST_EQ(ec2, std::error_code());
         BOOST_TEST_EQ(cont.pending_bytes(), 0u);

         // Check that the reader actually blocked
         co_await yield();
         BOOST_TEST_EQ(reader_point, 1);

         // The pushes have been consumed. Block for more
         auto [ec3] = co_await cont.take();
         BOOST_TEST_EQ(ec3, std::error_code());
         BOOST_TEST_EQ(cont.pending_bytes(), 0u);

         co_return {};
      }(),
      [&]() -> capy::io_task<> {
         // A push arrives. There is room, so we just try_put and don't block
         BOOST_TEST(cont.try_put(23u));

         // No more pushes arrive for now, allow the consumer to catch up
         co_await yield();
         BOOST_TEST_EQ(receiver_point, 1);

         // A burst of pushes arrives. It's too much and the controller blocks
         BOOST_TEST(cont.try_put(21u));
         BOOST_TEST(cont.try_put(8u));
         BOOST_TEST(cont.try_put(50u));
         BOOST_TEST_NOT(cont.try_put(10u));
         BOOST_TEST_EQ(cont.pending_bytes(), 79u);
         reader_point = 1;

         // Because the last try_put returned false, we block now
         auto [ec] = co_await cont.put(10u);
         BOOST_TEST_EQ(ec, std::error_code());
         BOOST_TEST_EQ(cont.pending_bytes(), 10u);

         co_return {};
      }());

   BOOST_TEST_EQ(ec, std::error_code());
}

}  // namespace

int main()
{
   run_coroutine_test(test_take_initial());
   run_coroutine_test(test_take_pending_bytes());
   run_coroutine_test(test_take_cancel());

   test_try_put_not_full();
   test_try_put_just_full();
   test_try_put_past_full();
   test_try_put_ctor_arg();

   run_coroutine_test(test_put_initial());
   run_coroutine_test(test_put_not_full());
   run_coroutine_test(test_put_full());
   run_coroutine_test(test_put_unblocks_take());
   run_coroutine_test(test_put_cancel());

   run_coroutine_test(test_full_cycle());

   return boost::report_errors();
}
