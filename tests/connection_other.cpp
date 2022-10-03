/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <iostream>
#include <boost/asio.hpp>
#include <boost/system/errc.hpp>
#include <boost/asio/experimental/as_tuple.hpp>

#define BOOST_TEST_MODULE low level
#include <boost/test/included/unit_test.hpp>

#include <aedis.hpp>
#include <aedis/src.hpp>

namespace net = boost::asio;

using aedis::resp3::request;
using aedis::adapt;
using connection = aedis::connection<>;
using endpoint = aedis::endpoint;
using error_code = boost::system::error_code;
using net::experimental::as_tuple;

#ifdef BOOST_ASIO_HAS_CO_AWAIT
#include <boost/asio/experimental/awaitable_operators.hpp>
using namespace net::experimental::awaitable_operators;

net::awaitable<void> send_after(std::shared_ptr<connection> db, std::chrono::milliseconds ms)
{
   net::steady_timer st{co_await net::this_coro::executor};
   st.expires_after(ms);
   co_await st.async_wait(net::use_awaitable);

   request req;
   req.push("CLIENT", "PAUSE", ms.count());

   auto [ec, n] = co_await db->async_exec(req, adapt(), as_tuple(net::use_awaitable));
   BOOST_TEST(!ec);
}

BOOST_AUTO_TEST_CASE(test_idle)
{
   std::cout << boost::unit_test::framework::current_test_case().p_name << std::endl;
   std::chrono::milliseconds ms{5000};

   {
      std::cout << "test_idle" << std::endl;
      connection::timeouts cfg;
      cfg.resolve_timeout = std::chrono::seconds{1};
      cfg.connect_timeout = std::chrono::seconds{1};
      cfg.ping_interval = std::chrono::seconds{1};

      net::io_context ioc;
      auto db = std::make_shared<connection>(ioc);

      net::co_spawn(ioc.get_executor(), send_after(db, ms), net::detached);

      endpoint ep{"127.0.0.1", "6379"};
      db->async_run(ep, cfg, [](auto ec){
         BOOST_CHECK_EQUAL(ec, aedis::error::idle_timeout);
      });

      ioc.run();
   }

   //----------------------------------------------------------------
   // Since we have paused the server above, we have to wait until the
   // server is responsive again, so as not to cause other tests to
   // fail.

   {
      net::io_context ioc;
      auto db = std::make_shared<connection>(ioc);
      connection::timeouts cfg;
      cfg.ping_interval = 2 * ms;
      cfg.resolve_timeout = 2 * ms;
      cfg.connect_timeout = 2 * ms;
      cfg.ping_interval = 2 * ms;
      cfg.resp3_handshake_timeout = 2 * ms;

      request req;
      req.push("QUIT");

      endpoint ep{"127.0.0.1", "6379"};
      db->async_run(ep, req, adapt(), cfg, [](auto ec, auto){
         BOOST_CHECK_EQUAL(ec, net::error::misc_errors::eof);
      });

      ioc.run();
   }
}

net::awaitable<void> reconnect(std::shared_ptr<connection> db)
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

BOOST_AUTO_TEST_CASE(test_cancelation)
{
   std::cout << boost::unit_test::framework::current_test_case().p_name << std::endl;
   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc);
   net::co_spawn(ioc, reconnect(db), net::detached);
   ioc.run();
}
#endif

BOOST_AUTO_TEST_CASE(test_wrong_data_type)
{
   std::cout << boost::unit_test::framework::current_test_case().p_name << std::endl;
   request req;
   req.push("QUIT");

   // Wrong data type.
   std::tuple<int> resp;
   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc);
   endpoint ep{"127.0.0.1", "6379"};
   db->async_run(ep, req, adapt(resp), {}, [](auto ec, auto){
      BOOST_CHECK_EQUAL(ec, aedis::error::not_a_number);
   });

   ioc.run();
}

BOOST_AUTO_TEST_CASE(test_not_connected)
{
   std::cout << boost::unit_test::framework::current_test_case().p_name << std::endl;
   request req{{true}};
   req.push("PING");

   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc);
   db->async_exec(req, adapt(), [](auto ec, auto){
      BOOST_CHECK_EQUAL(ec, aedis::error::not_connected);
   });

   ioc.run();
}
