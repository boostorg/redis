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
#include "common.hpp"
#include "../examples/common/common.hpp"

namespace net = boost::asio;
namespace resp3 = aedis::resp3;

using aedis::adapt;
using error_code = boost::system::error_code;

#include <boost/asio/experimental/awaitable_operators.hpp>
using namespace boost::asio::experimental::awaitable_operators;

net::awaitable<void> test_reconnect_impl()
{
   auto ex = co_await net::this_coro::executor;

   resp3::request req;
   req.push("QUIT");

   auto const endpoints = resolve();
   connection conn{ex};

   int i = 0;
   for (; i < 5; ++i) {
      boost::system::error_code ec1, ec2;
      net::connect(conn.next_layer(), endpoints);
      co_await (
         conn.async_exec(req, adapt(), net::redirect_error(net::use_awaitable, ec1)) &&
         conn.async_run(net::redirect_error(net::use_awaitable, ec2))
      );

      BOOST_TEST(!ec1);
      BOOST_TEST(!ec2);
      conn.reset_stream();
   }

   BOOST_CHECK_EQUAL(i, 5);
   co_return;
}

// Test whether the client works after a reconnect.
BOOST_AUTO_TEST_CASE(test_reconnect)
{
   net::io_context ioc;
   net::co_spawn(ioc, test_reconnect_impl(), net::detached);
   ioc.run();
}

auto async_test_reconnect_timeout() -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;

   net::steady_timer st{ex};

   auto conn = std::make_shared<connection>(ex);
   boost::system::error_code ec1, ec2, ec3;

   resp3::request req1;
   req1.get_config().cancel_if_not_connected = false;
   req1.get_config().cancel_on_connection_lost = true;
   req1.get_config().cancel_if_unresponded = true;
   req1.push("HELLO", 3);
   req1.push("BLPOP", "any", 0);

   co_await connect(conn, "127.0.0.1", "6379");
   st.expires_after(std::chrono::seconds{1});
   co_await (
      conn->async_exec(req1, adapt(), redir(ec1)) ||
      conn->async_run(redir(ec2)) ||
      st.async_wait(redir(ec3))
   );

   //BOOST_TEST(!ec1);
   BOOST_CHECK_EQUAL(ec2, boost::system::errc::errc_t::operation_canceled);
   //BOOST_TEST(!ec3);

   resp3::request req2;
   req2.get_config().cancel_if_not_connected = false;
   req2.get_config().cancel_on_connection_lost = true;
   req2.get_config().cancel_if_unresponded= true;
   req2.push("HELLO", 3);
   req2.push("QUIT");

   co_await connect(conn, "127.0.0.1", "6379");
   st.expires_after(std::chrono::seconds{1});
   co_await (
      conn->async_exec(req1, adapt(), net::redirect_error(net::use_awaitable, ec1)) ||
      conn->async_run(net::redirect_error(net::use_awaitable, ec2)) ||
      st.async_wait(net::redirect_error(net::use_awaitable, ec3))
   );
   std::cout << "ccc" << std::endl;

   BOOST_CHECK_EQUAL(ec1, boost::system::errc::errc_t::operation_canceled);
   BOOST_CHECK_EQUAL(ec2, boost::asio::error::basic_errors::operation_aborted);
}

BOOST_AUTO_TEST_CASE(test_reconnect_and_idle)
{
   run(async_test_reconnect_timeout());
}
#else
int main(){}
#endif
