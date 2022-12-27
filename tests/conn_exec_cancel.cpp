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
#define BOOST_TEST_MODULE low level
#include <boost/test/included/unit_test.hpp>
#include <aedis.hpp>
#include <aedis/src.hpp>
#include "common.hpp"
#include "../examples/common/common.hpp"

// NOTE1: Sends hello separately. I have observed that if hello and
// blpop are sent toguether, Redis will send the response of hello
// right away, not waiting for blpop. That is why we have to send it
// separately here.

namespace net = boost::asio;
namespace resp3 = aedis::resp3;
using error_code = boost::system::error_code;
using namespace net::experimental::awaitable_operators;
using aedis::operation;
using aedis::adapt;

auto async_ignore_explicit_cancel_of_req_written() -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;

   auto conn = std::make_shared<connection>(ex);
   co_await connect(conn, "127.0.0.1", "6379");

   conn->async_run([conn](auto ec) {
      BOOST_TEST(!ec);
   });

   net::steady_timer st{ex};
   st.expires_after(std::chrono::seconds{1});

   // See NOTE1.
   resp3::request req0;
   req0.get_config().coalesce = false;
   req0.push("HELLO", 3);
   std::ignore = co_await conn->async_exec(req0, adapt(), net::use_awaitable);

   resp3::request req1;
   req1.get_config().coalesce = false;
   req1.push("BLPOP", "any", 3);

   // Should not be canceled.
   conn->async_exec(req1, adapt(), [](auto ec, auto){
      BOOST_TEST(!ec);
   });

   resp3::request req2;
   req2.get_config().coalesce = false;
   req2.push("PING", "second");

   // Should be canceled.
   conn->async_exec(req2, adapt(), [](auto ec, auto){
      BOOST_CHECK_EQUAL(ec, net::error::basic_errors::operation_aborted);
   });

   // Will complete while BLPOP is pending.
   boost::system::error_code ec1;
   co_await st.async_wait(net::redirect_error(net::use_awaitable, ec1));
   conn->cancel(operation::exec);

   BOOST_TEST(!ec1);

   resp3::request req3;
   req3.push("QUIT");

   // Test whether the connection remains usable after a call to
   // cancel(exec).
   co_await conn->async_exec(req3, adapt(), net::redirect_error(net::use_awaitable, ec1));

   BOOST_TEST(!ec1);
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
   resp3::request req0;
   req0.get_config().coalesce = false;
   req0.push("HELLO", 3);
   std::ignore = co_await conn->async_exec(req0, adapt(), net::use_awaitable);

   // Will be cancelled after it has been written but before the
   // response arrives.
   resp3::request req1;
   req1.get_config().coalesce = false;
   req1.push("BLPOP", "any", 3);

   // Will be cancelled before it is written.
   resp3::request req2;
   req2.get_config().coalesce = false;
   req2.get_config().cancel_on_connection_lost = true;
   req2.push("PING");

   net::steady_timer st{ex};
   st.expires_after(std::chrono::seconds{1});

   boost::system::error_code ec1, ec2, ec3;
   co_await (
      conn->async_exec(req1, adapt(), redir(ec1)) ||
      conn->async_exec(req2, adapt(), redir(ec2)) ||
      st.async_wait(redir(ec3))
   );

   BOOST_CHECK_EQUAL(ec1, net::error::basic_errors::operation_aborted);
   BOOST_CHECK_EQUAL(ec2, net::error::basic_errors::operation_aborted);
   BOOST_TEST(!ec3);
}

auto cancel_of_req_written_on_run_canceled() -> net::awaitable<void>
{
   resp3::request req0;
   req0.get_config().coalesce = false;
   req0.push("HELLO", 3);

   resp3::request req1;
   req1.get_config().cancel_on_connection_lost = true;
   req1.get_config().cancel_if_unresponded = true;
   req1.push("BLPOP", "any", 0);

   auto ex = co_await net::this_coro::executor;
   auto conn = std::make_shared<connection>(ex);
   co_await connect(conn, "127.0.0.1", "6379");

   net::steady_timer st{ex};
   st.expires_after(std::chrono::seconds{1});

   boost::system::error_code ec0, ec1, ec2, ec3;
   co_await (
      conn->async_exec(req0, adapt(), redir(ec0)) &&
      (conn->async_exec(req1, adapt(), redir(ec1)) ||
      conn->async_run(redir(ec2)) ||
      st.async_wait(redir(ec3)))
   );

   BOOST_TEST(!ec0);
   BOOST_CHECK_EQUAL(ec1, net::error::basic_errors::operation_aborted);
   BOOST_CHECK_EQUAL(ec2, net::error::basic_errors::operation_aborted);
   BOOST_TEST(!ec3);
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
   run(cancel_of_req_written_on_run_canceled());
}

#else
int main(){}
#endif
