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
   std::cout << boost::unit_test::framework::current_test_case().p_name << std::endl;
   net::io_context ioc;
   net::co_spawn(ioc.get_executor(), async_cancel_run_with_timer(), net::detached);
   ioc.run();
}

net::awaitable<void> async_cancel_run_with_timer_stress(std::shared_ptr<connection> db)
{
   net::steady_timer timer{co_await net::this_coro::executor};
   for (auto i = 0; i < 1000; ++i) {
      timer.expires_after(std::chrono::milliseconds{10});
      endpoint ep{"127.0.0.1", "6379"};
      co_await (
         db->async_run(ep, {}, net::use_awaitable) ||
         timer.async_wait(net::use_awaitable)
      );
      std::cout << i << ": Retrying" << std::endl;
   }
   std::cout << "Finished" << std::endl;
}

BOOST_AUTO_TEST_CASE(cancel_run_with_timer_stress)
{
   std::cout << boost::unit_test::framework::current_test_case().p_name << std::endl;
   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc);
   net::co_spawn(ioc, async_cancel_run_with_timer_stress(db), net::detached);
   ioc.run();
}

net::awaitable<void> async_cancel_run_with_timer_stress2()
{
   auto ex =  co_await net::this_coro::executor;
   
   auto db0 = connection{ex};
   auto db1 = connection{ex};
   auto db2 = connection{ex};
   auto db3 = connection{ex};
   auto db4 = connection{ex};
   auto db5 = connection{ex};
   auto db6 = connection{ex};
   auto db7 = connection{ex};
   auto db8 = connection{ex};
   auto db9 = connection{ex};

   auto timer = net::steady_timer{ex};
   
   connection::timeouts tms;
   tms.resolve_timeout = std::chrono::seconds{10};
   tms.connect_timeout = std::chrono::seconds{10};
   tms.resp3_handshake_timeout = std::chrono::seconds{2};
   tms.ping_interval = std::chrono::seconds{1};

   endpoint ep{ "127.0.0.1", "6379" };

   for (int i = 0; i < 30; i++) {
      timer.expires_after(std::chrono::milliseconds{1005}); // Note extra 5ms
      co_await (
         db0.async_run(ep, tms, net::use_awaitable) ||
         db1.async_run(ep, tms, net::use_awaitable) ||
         db2.async_run(ep, tms, net::use_awaitable) ||
         db3.async_run(ep, tms, net::use_awaitable) ||
         db4.async_run(ep, tms, net::use_awaitable) ||
         db5.async_run(ep, tms, net::use_awaitable) ||
         db6.async_run(ep, tms, net::use_awaitable) ||
         db7.async_run(ep, tms, net::use_awaitable) ||
         db8.async_run(ep, tms, net::use_awaitable) ||
         db9.async_run(ep, tms, net::use_awaitable) ||
         timer.async_wait(net::use_awaitable)
      );
      std::cout << i << std::endl;
   }
}

// PR #29
BOOST_AUTO_TEST_CASE(cancel_run_with_timer_stress2)
{
   std::cout << boost::unit_test::framework::current_test_case().p_name << std::endl;
   net::io_context ioc;
   net::co_spawn(ioc, async_cancel_run_with_timer_stress2(), net::detached);
   ioc.run();
}

#else
int main(){}
#endif
