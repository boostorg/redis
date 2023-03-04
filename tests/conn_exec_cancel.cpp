/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <iostream>
#include <boost/asio.hpp>
#ifdef BOOST_ASIO_HAS_CO_AWAIT
#include <boost/system/errc.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#define BOOST_TEST_MODULE conn-exec-cancel
#include <boost/test/included/unit_test.hpp>
#include <boost/redis.hpp>
#include <boost/redis/src.hpp>
#include "common.hpp"
#include "../examples/common/common.hpp"

// NOTE1: Sends hello separately. I have observed that if hello and
// blpop are sent toguether, Redis will send the response of hello
// right away, not waiting for blpop. That is why we have to send it
// separately here.

namespace net = boost::asio;
using error_code = boost::system::error_code;
using namespace net::experimental::awaitable_operators;
using boost::redis::operation;
using boost::redis::request;
using boost::redis::response;
using boost::redis::generic_response;
using boost::redis::ignore;
using boost::redis::ignore_t;

auto async_ignore_explicit_cancel_of_req_written() -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;

   generic_response gresp;
   auto conn = std::make_shared<connection>(ex);
   co_await connect(conn, "127.0.0.1", "6379");

   conn->async_run([conn](auto ec) {
      std::cout << "async_run: " << ec.message() << std::endl;
      BOOST_TEST(!ec);
   });

   net::steady_timer st{ex};
   st.expires_after(std::chrono::seconds{1});

   // See NOTE1.
   request req0;
   req0.push("HELLO", 3);
   co_await conn->async_exec(req0, gresp, net::use_awaitable);

   request req1;
   req1.push("BLPOP", "any", 3);

   // Should not be canceled.
   bool seen = false;
   conn->async_exec(req1, gresp, [&](auto ec, auto) mutable{
      std::cout << "async_exec (1): " << ec.message() << std::endl;
      BOOST_TEST(!ec);
      seen = true;
   });

   // Will complete while BLPOP is pending.
   boost::system::error_code ec1;
   co_await st.async_wait(net::redirect_error(net::use_awaitable, ec1));
   conn->cancel(operation::exec);

   BOOST_TEST(!ec1);

   request req3;
   req3.push("QUIT");

   // Test whether the connection remains usable after a call to
   // cancel(exec).
   co_await conn->async_exec(req3, gresp, net::redirect_error(net::use_awaitable, ec1));

   BOOST_TEST(!ec1);
   BOOST_TEST(seen);
}

auto ignore_implicit_cancel_of_req_written() -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;
   auto conn = std::make_shared<connection>(ex);
   co_await connect(conn, "127.0.0.1", "6379");

   // Calls async_run separately from the group of ops below to avoid
   // having it canceled when the timer fires.
   conn->async_run([conn](auto ec) {
      BOOST_CHECK_EQUAL(ec, net::error::basic_errors::operation_aborted);
   });

   // See NOTE1.
   request req0;
   req0.push("HELLO", 3);
   co_await conn->async_exec(req0, ignore, net::use_awaitable);

   // Will be cancelled after it has been written but before the
   // response arrives.
   request req1;
   req1.push("BLPOP", "any", 3);

   net::steady_timer st{ex};
   st.expires_after(std::chrono::seconds{1});

   boost::system::error_code ec1, ec2;
   co_await (
      conn->async_exec(req1, ignore, redir(ec1)) ||
      st.async_wait(redir(ec2))
   );

   BOOST_CHECK_EQUAL(ec1, net::error::basic_errors::operation_aborted);
   BOOST_TEST(!ec2);
}

BOOST_AUTO_TEST_CASE(test_ignore_explicit_cancel_of_req_written)
{
   run(async_ignore_explicit_cancel_of_req_written());
}

BOOST_AUTO_TEST_CASE(test_ignore_implicit_cancel_of_req_written)
{
   run(ignore_implicit_cancel_of_req_written());
}

BOOST_AUTO_TEST_CASE(test_cancel_of_req_written_on_run_canceled)
{
   net::io_context ioc;
   auto const endpoints = resolve();
   connection conn{ioc};
   net::connect(conn.next_layer(), endpoints);

   request req0;
   req0.push("HELLO", 3);

   // Sends a request that will be blocked forever, so we can test
   // canceling it while waiting for a response.
   request req1;
   req1.get_config().cancel_on_connection_lost = true;
   req1.get_config().cancel_if_unresponded = true;
   req1.push("BLPOP", "any", 0);

   auto c1 = [&](auto ec, auto)
   {
      BOOST_CHECK_EQUAL(ec, net::error::basic_errors::operation_aborted);
   };

   auto c0 = [&](auto ec, auto)
   {
      BOOST_TEST(!ec);
      conn.async_exec(req1, ignore, c1);
   };

   conn.async_exec(req0, ignore, c0);

   conn.async_run([](auto ec){
      BOOST_CHECK_EQUAL(ec, net::error::basic_errors::operation_aborted);
   });

   net::steady_timer st{ioc};
   st.expires_after(std::chrono::seconds{1});
   st.async_wait([&](auto ec){
      BOOST_TEST(!ec);
      conn.cancel(operation::run);
   });

   ioc.run();
}

#else
int main(){}
#endif
