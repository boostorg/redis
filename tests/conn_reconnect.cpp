/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <iostream>
#include <boost/asio.hpp>
#ifdef BOOST_ASIO_HAS_CO_AWAIT

#define BOOST_TEST_MODULE low level
#include <boost/test/included/unit_test.hpp>

#include <aedis.hpp>
#include <aedis/src.hpp>

namespace net = boost::asio;

using aedis::resp3::request;
using aedis::adapt;
using connection = aedis::connection<>;
using error_code = boost::system::error_code;

#include <boost/asio/experimental/awaitable_operators.hpp>
using namespace boost::asio::experimental::awaitable_operators;

net::awaitable<void> test_reconnect_impl(std::shared_ptr<connection> db)
{
   request req;
   req.push("QUIT");

   int i = 0;
   for (; i < 5; ++i) {
      boost::system::error_code ec1, ec2;
      co_await (
         db->async_exec(req, adapt(), net::redirect_error(net::use_awaitable, ec1)) &&
         db->async_run("127.0.0.1", "6379", {}, net::redirect_error(net::use_awaitable, ec2))
      );

      BOOST_TEST(!ec1);
      BOOST_TEST(!ec2);
      db->reset_stream();
   }

   BOOST_CHECK_EQUAL(i, 5);
   co_return;
}

// Test whether the client works after a reconnect.
BOOST_AUTO_TEST_CASE(test_reconnect)
{
   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc);
   net::co_spawn(ioc, test_reconnect_impl(db), net::detached);
   ioc.run();
}

auto async_test_reconnect_timeout() -> net::awaitable<void>
{
   auto conn = std::make_shared<connection>(co_await net::this_coro::executor);
   boost::system::error_code ec1, ec2;

   request req1;
   req1.get_config().cancel_if_not_connected = false;
   req1.get_config().cancel_on_connection_lost = true;
   req1.push("HELLO", 3);
   req1.push("CLIENT", "PAUSE", 7000);

   co_await (
      conn->async_exec(req1, adapt(), net::redirect_error(net::use_awaitable, ec1)) &&
      conn->async_run("127.0.0.1", "6379", {}, net::redirect_error(net::use_awaitable, ec2))
   );

   BOOST_TEST(!ec1);
   BOOST_CHECK_EQUAL(ec2, aedis::error::idle_timeout);

   request req2;
   req2.get_config().cancel_if_not_connected = false;
   req2.get_config().cancel_on_connection_lost = true;
   req2.push("HELLO", 3);
   req2.push("QUIT");

   co_await (
      conn->async_exec(req1, adapt(), net::redirect_error(net::use_awaitable, ec1)) &&
      conn->async_run("127.0.0.1", "6379", {}, net::redirect_error(net::use_awaitable, ec2))
   );

   BOOST_CHECK_EQUAL(ec1, boost::system::errc::errc_t::operation_canceled);
   BOOST_CHECK_EQUAL(ec2, aedis::error::idle_timeout);
}

BOOST_AUTO_TEST_CASE(test_reconnect_and_idle)
{
   net::io_context ioc;
   net::co_spawn(ioc, async_test_reconnect_timeout(), net::detached);
   ioc.run();
}
#else
int main(){}
#endif
