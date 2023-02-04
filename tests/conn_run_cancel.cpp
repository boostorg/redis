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

#include <boost/redis.hpp>
#include <boost/redis/src.hpp>
#include "common.hpp"

namespace net = boost::asio;

using boost::redis::operation;
using boost::redis::adapt;
using connection = boost::redis::connection;
using error_code = boost::system::error_code;
using net::experimental::as_tuple;
using boost::redis::request;
using boost::redis::response;

#include <boost/asio/experimental/awaitable_operators.hpp>
using namespace net::experimental::awaitable_operators;

auto async_cancel_run_with_timer() -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;
   auto const endpoints = resolve();
   connection conn{ex};
   net::connect(conn.next_layer(), endpoints);

   net::steady_timer st{ex};
   st.expires_after(std::chrono::seconds{1});

   boost::system::error_code ec1, ec2;
   co_await (conn.async_run(redir(ec1)) || st.async_wait(redir(ec2)));

   BOOST_CHECK_EQUAL(ec1, boost::asio::error::basic_errors::operation_aborted);
   BOOST_TEST(!ec2);
}

BOOST_AUTO_TEST_CASE(cancel_run_with_timer)
{
   net::io_context ioc;
   net::co_spawn(ioc.get_executor(), async_cancel_run_with_timer(), net::detached);
   ioc.run();
}

auto
async_check_cancellation_not_missed(int n, std::chrono::milliseconds ms) -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;
   auto const endpoints = resolve();
   connection conn{ex};

   net::steady_timer timer{ex};

   for (auto i = 0; i < n; ++i) {
      timer.expires_after(ms);
      net::connect(conn.next_layer(), endpoints);
      boost::system::error_code ec1, ec2;
      co_await (conn.async_run(redir(ec1)) || timer.async_wait(redir(ec2)));
      BOOST_CHECK_EQUAL(ec1, boost::asio::error::basic_errors::operation_aborted);
      std::cout << "Counter: " << i << std::endl;
   }
}

// See PR #29
BOOST_AUTO_TEST_CASE(check_implicit_cancel_not_missed_0)
{
   net::io_context ioc;
   net::co_spawn(ioc, async_check_cancellation_not_missed(10, std::chrono::milliseconds{0}), net::detached);
   ioc.run();
}

BOOST_AUTO_TEST_CASE(check_implicit_cancel_not_missed_2)
{
   net::io_context ioc;
   net::co_spawn(ioc, async_check_cancellation_not_missed(20, std::chrono::milliseconds{2}), net::detached);
   ioc.run();
}

BOOST_AUTO_TEST_CASE(check_implicit_cancel_not_missed_8)
{
   net::io_context ioc;
   net::co_spawn(ioc, async_check_cancellation_not_missed(20, std::chrono::milliseconds{8}), net::detached);
   ioc.run();
}

BOOST_AUTO_TEST_CASE(check_implicit_cancel_not_missed_16)
{
   net::io_context ioc;
   net::co_spawn(ioc, async_check_cancellation_not_missed(20, std::chrono::milliseconds{16}), net::detached);
   ioc.run();
}

BOOST_AUTO_TEST_CASE(check_implicit_cancel_not_missed_32)
{
   net::io_context ioc;
   net::co_spawn(ioc, async_check_cancellation_not_missed(20, std::chrono::milliseconds{32}), net::detached);
   ioc.run();
}

BOOST_AUTO_TEST_CASE(check_implicit_cancel_not_missed_64)
{
   net::io_context ioc;
   net::co_spawn(ioc, async_check_cancellation_not_missed(20, std::chrono::milliseconds{64}), net::detached);
   ioc.run();
}

BOOST_AUTO_TEST_CASE(check_implicit_cancel_not_missed_128)
{
   net::io_context ioc;
   net::co_spawn(ioc, async_check_cancellation_not_missed(20, std::chrono::milliseconds{128}), net::detached);
   ioc.run();
}

BOOST_AUTO_TEST_CASE(check_implicit_cancel_not_missed_256)
{
   net::io_context ioc;
   net::co_spawn(ioc, async_check_cancellation_not_missed(20, std::chrono::milliseconds{256}), net::detached);
   ioc.run();
}

BOOST_AUTO_TEST_CASE(check_implicit_cancel_not_missed_512)
{
   net::io_context ioc;
   net::co_spawn(ioc, async_check_cancellation_not_missed(20, std::chrono::milliseconds{512}), net::detached);
   ioc.run();
}

BOOST_AUTO_TEST_CASE(check_implicit_cancel_not_missed_1024)
{
   net::io_context ioc;
   net::co_spawn(ioc, async_check_cancellation_not_missed(20, std::chrono::milliseconds{1024}), net::detached);
   ioc.run();
}

BOOST_AUTO_TEST_CASE(reset_before_run_completes)
{
   net::io_context ioc;
   auto const endpoints = resolve();
   connection conn{ioc};
   net::connect(conn.next_layer(), endpoints);


   // Sends a ping just as a means of waiting until we are connected.
   request req;
   req.push("HELLO", 3);
   req.push("PING");

   conn.async_exec(req, adapt(), [&](auto ec, auto){
      BOOST_TEST(!ec);
      conn.reset_stream();
   });

   conn.async_run([&](auto ec){
      BOOST_CHECK_EQUAL(ec, net::error::operation_aborted);
   });

   ioc.run();
}

#else
int main(){}
#endif
