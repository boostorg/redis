/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <iostream>
#include <boost/asio.hpp>
#ifdef BOOST_ASIO_HAS_CO_AWAIT
#include <boost/system/errc.hpp>
#include <boost/asio/experimental/as_tuple.hpp>

#define BOOST_TEST_MODULE low level
#include <boost/test/included/unit_test.hpp>

#include <aedis.hpp>
#include <aedis/src.hpp>

namespace net = boost::asio;

using aedis::resp3::request;
using aedis::operation;
using aedis::adapt;
using connection = aedis::connection<>;
using endpoint = aedis::endpoint;
using error_code = boost::system::error_code;
using net::experimental::as_tuple;

#include <boost/asio/experimental/awaitable_operators.hpp>
using namespace net::experimental::awaitable_operators;

auto async_cancel_run_with_timer() -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;
   auto conn = std::make_shared<connection>(ex);
   net::steady_timer st{ex};
   st.expires_after(std::chrono::seconds{1});

   endpoint ep{"127.0.0.1", "6379"};
   boost::system::error_code ec1, ec2;
   co_await (
      conn->async_run(ep, {}, net::redirect_error(net::use_awaitable, ec1)) ||
      st.async_wait(net::redirect_error(net::use_awaitable, ec2))
   );

   BOOST_CHECK_EQUAL(ec1, boost::asio::error::basic_errors::operation_aborted);
   BOOST_TEST(!ec2);
}

BOOST_AUTO_TEST_CASE(cancel_run_with_timer)
{
   net::io_context ioc;
   net::co_spawn(ioc.get_executor(), async_cancel_run_with_timer(), net::detached);
   ioc.run();
}

net::awaitable<void>
async_check_cancellation_not_missed(
   std::shared_ptr<connection> conn,
   int n,
   std::chrono::milliseconds ms)
{
   net::steady_timer timer{co_await net::this_coro::executor};

   connection::timeouts tms;
   tms.resolve_timeout = std::chrono::seconds{10};
   tms.connect_timeout = std::chrono::seconds{10};
   tms.resp3_handshake_timeout = std::chrono::seconds{2};
   tms.ping_interval = std::chrono::seconds{1};

   endpoint ep{"127.0.0.1", "6379"};

   for (auto i = 0; i < n; ++i) {
      timer.expires_after(ms);
      boost::system::error_code ec1, ec2;
      co_await (
         conn->async_run(ep, {}, net::redirect_error(net::use_awaitable, ec1)) ||
         timer.async_wait(net::redirect_error(net::use_awaitable, ec2))
      );
      BOOST_CHECK_EQUAL(ec1, boost::asio::error::basic_errors::operation_aborted);
      std::cout << "Counter: " << i << std::endl;
   }
}

// See PR #29
BOOST_AUTO_TEST_CASE(check_implicit_cancel_not_missed_0)
{
   net::io_context ioc;
   auto conn = std::make_shared<connection>(ioc);
   net::co_spawn(ioc, async_check_cancellation_not_missed(conn, 10, std::chrono::milliseconds{0}), net::detached);
   ioc.run();
}

BOOST_AUTO_TEST_CASE(check_implicit_cancel_not_missed_2)
{
   net::io_context ioc;
   auto conn = std::make_shared<connection>(ioc);
   net::co_spawn(ioc, async_check_cancellation_not_missed(conn, 20, std::chrono::milliseconds{2}), net::detached);
   ioc.run();
}

BOOST_AUTO_TEST_CASE(check_implicit_cancel_not_missed_8)
{
   net::io_context ioc;
   auto conn = std::make_shared<connection>(ioc);
   net::co_spawn(ioc, async_check_cancellation_not_missed(conn, 20, std::chrono::milliseconds{8}), net::detached);
   ioc.run();
}

BOOST_AUTO_TEST_CASE(check_implicit_cancel_not_missed_16)
{
   net::io_context ioc;
   auto conn = std::make_shared<connection>(ioc);
   net::co_spawn(ioc, async_check_cancellation_not_missed(conn, 20, std::chrono::milliseconds{16}), net::detached);
   ioc.run();
}

BOOST_AUTO_TEST_CASE(check_implicit_cancel_not_missed_32)
{
   net::io_context ioc;
   auto conn = std::make_shared<connection>(ioc);
   net::co_spawn(ioc, async_check_cancellation_not_missed(conn, 20, std::chrono::milliseconds{32}), net::detached);
   ioc.run();
}

BOOST_AUTO_TEST_CASE(check_implicit_cancel_not_missed_64)
{
   net::io_context ioc;
   auto conn = std::make_shared<connection>(ioc);
   net::co_spawn(ioc, async_check_cancellation_not_missed(conn, 20, std::chrono::milliseconds{64}), net::detached);
   ioc.run();
}

BOOST_AUTO_TEST_CASE(check_implicit_cancel_not_missed_128)
{
   net::io_context ioc;
   auto conn = std::make_shared<connection>(ioc);
   net::co_spawn(ioc, async_check_cancellation_not_missed(conn, 20, std::chrono::milliseconds{128}), net::detached);
   ioc.run();
}

BOOST_AUTO_TEST_CASE(check_implicit_cancel_not_missed_256)
{
   net::io_context ioc;
   auto conn = std::make_shared<connection>(ioc);
   net::co_spawn(ioc, async_check_cancellation_not_missed(conn, 20, std::chrono::milliseconds{256}), net::detached);
   ioc.run();
}

BOOST_AUTO_TEST_CASE(check_implicit_cancel_not_missed_512)
{
   net::io_context ioc;
   auto conn = std::make_shared<connection>(ioc);
   net::co_spawn(ioc, async_check_cancellation_not_missed(conn, 20, std::chrono::milliseconds{512}), net::detached);
   ioc.run();
}

BOOST_AUTO_TEST_CASE(check_implicit_cancel_not_missed_1024)
{
   net::io_context ioc;
   auto conn = std::make_shared<connection>(ioc);
   net::co_spawn(ioc, async_check_cancellation_not_missed(conn, 20, std::chrono::milliseconds{1024}), net::detached);
   ioc.run();
}

BOOST_AUTO_TEST_CASE(reset_before_run_completes)
{
   net::io_context ioc;
   auto conn = std::make_shared<connection>(ioc);

   // Sends a ping just as a means of waiting until we are connected.
   request req;
   req.push("PING");

   conn->async_exec(req, adapt(), [conn](auto ec, auto){
      BOOST_TEST(!ec);
      conn->reset_stream();
   });

   conn->async_run({"127.0.0.1", "6379"}, {}, [conn](auto ec){
      BOOST_CHECK_EQUAL(ec, net::error::operation_aborted);
   });

   ioc.run();
}

#else
int main(){}
#endif
