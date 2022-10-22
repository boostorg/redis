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

auto async_run(std::shared_ptr<connection> conn) -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;

   connection::timeouts tms;
   tms.ping_interval = std::chrono::seconds{10};
   endpoint ep{"127.0.0.1", "6379"};
   boost::system::error_code ec;
   co_await conn->async_run(ep, tms, net::redirect_error(net::use_awaitable, ec));
   BOOST_CHECK_EQUAL(ec, net::error::misc_errors::eof);
}

auto async_cancel_exec(std::shared_ptr<connection> conn) -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;

   net::steady_timer st{ex};
   st.expires_after(std::chrono::seconds{1});

   boost::system::error_code ec1;

   request req1;
   req1.get_config().coalesce = false;
   req1.push("BLPOP", "any", 3);

   // Should not be canceled.
   conn->async_exec(req1, adapt(), [](auto ec, auto){
      BOOST_TEST(!ec);
   });

   request req2;
   req2.get_config().coalesce = false;
   req2.push("PING", "second");

   // Should be canceled.
   conn->async_exec(req1, adapt(), [](auto ec, auto){
      BOOST_CHECK_EQUAL(ec, boost::asio::error::basic_errors::operation_aborted);
   });

   // Will complete while BLPOP is pending.
   co_await st.async_wait(net::redirect_error(net::use_awaitable, ec1));
   conn->cancel(operation::exec);

   BOOST_TEST(!ec1);

   request req3;
   req3.push("QUIT");

   // Test whether the connection remains usable after a call to
   // cancel(exec).
   co_await conn->async_exec(req3, adapt(), net::redirect_error(net::use_awaitable, ec1));

   BOOST_TEST(!ec1);
}

BOOST_AUTO_TEST_CASE(cancel_exec_with_timer)
{
   std::cout << boost::unit_test::framework::current_test_case().p_name << std::endl;
   net::io_context ioc;
   auto conn = std::make_shared<connection>(ioc);
   net::co_spawn(ioc.get_executor(), async_run(conn), net::detached);
   net::co_spawn(ioc.get_executor(), async_cancel_exec(conn), net::detached);
   ioc.run();
}

auto async_ignore_cancel_of_written_req(std::shared_ptr<connection> conn) -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;

   net::steady_timer st{ex};
   st.expires_after(std::chrono::seconds{1});

   net::steady_timer st2{ex};
   st2.expires_after(std::chrono::seconds{3});

   boost::system::error_code ec1, ec2, ec3;

   request req1; // Will be cancelled after it has been written.
   req1.get_config().coalesce = false;
   req1.push("BLPOP", "any", 3);

   request req2; // Will be cancelled.
   req2.push("PING");

   co_await (
      conn->async_exec(req1, adapt(), net::redirect_error(net::use_awaitable, ec1)) ||
      conn->async_exec(req2, adapt(), net::redirect_error(net::use_awaitable, ec2)) ||
      st.async_wait(net::redirect_error(net::use_awaitable, ec3))
   );

   BOOST_TEST(!ec1);
   BOOST_CHECK_EQUAL(ec2, boost::asio::error::basic_errors::operation_aborted);
   BOOST_TEST(!ec3);

   request req3;
   req3.push("PING");
   req3.push("QUIT");
   co_await conn->async_exec(req3, adapt(), net::redirect_error(net::use_awaitable, ec1));
   BOOST_TEST(!ec1);
}

BOOST_AUTO_TEST_CASE(ignore_cancel_of_written_req)
{
   std::cout << boost::unit_test::framework::current_test_case().p_name << std::endl;
   net::io_context ioc;
   auto conn = std::make_shared<connection>(ioc);
   net::co_spawn(ioc.get_executor(), async_run(conn), net::detached);
   net::co_spawn(ioc.get_executor(), async_ignore_cancel_of_written_req(conn), net::detached);
   ioc.run();
}
#else
int main(){}
#endif
