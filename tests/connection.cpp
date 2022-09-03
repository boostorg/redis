/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

// TODO: Avoid usage of co_await to improve tests is compilers that
// don't support it.
// TODO: Add reconnect test that kills the server and waits some
// seconds.

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

bool is_host_not_found(boost::system::error_code ec)
{
   if (ec == net::error::netdb_errors::host_not_found) return true;
   if (ec == net::error::netdb_errors::host_not_found_try_again) return true;
   return false;
}

//----------------------------------------------------------------

// Tests whether resolve fails with the correct error.
BOOST_AUTO_TEST_CASE(test_resolve)
{
   std::cout << boost::unit_test::framework::current_test_case().p_name << std::endl;
   endpoint ep;
   ep.host = "Atibaia";
   ep.port = "6379";

   net::io_context ioc;
   connection db{ioc};
   db.get_config().resolve_timeout = std::chrono::seconds{100};
   db.async_run(ep, [](auto ec) {
      BOOST_TEST(is_host_not_found(ec));
   });

   ioc.run();
}

//----------------------------------------------------------------

BOOST_AUTO_TEST_CASE(test_resolve_with_timeout)
{
   std::cout << boost::unit_test::framework::current_test_case().p_name << std::endl;
   net::io_context ioc;

   connection db{ioc};
   endpoint ep;
   ep.host = "Atibaia";
   ep.port = "6379";

   // Low-enough to cause a timeout always.
   db.get_config().resolve_timeout = std::chrono::milliseconds{1};

   db.async_run(ep, [](auto const& ec) {
      BOOST_CHECK_EQUAL(ec, aedis::error::resolve_timeout);
   });

   ioc.run();
}

//----------------------------------------------------------------

BOOST_AUTO_TEST_CASE(test_connect)
{
   std::cout << boost::unit_test::framework::current_test_case().p_name << std::endl;

   endpoint ep;
   ep.host = "127.0.0.1";
   ep.port = "1";

   net::io_context ioc;
   connection db{ioc};
   db.get_config().connect_timeout = std::chrono::seconds{100};
   db.async_run(ep, [](auto ec) {
      BOOST_CHECK_EQUAL(ec, net::error::basic_errors::connection_refused);
   });
   ioc.run();
}

//----------------------------------------------------------------

BOOST_AUTO_TEST_CASE(test_connect_timeout)
{
   std::cout << boost::unit_test::framework::current_test_case().p_name << std::endl;
   net::io_context ioc;

   endpoint ep;
   ep.host = "example.com";
   ep.port = "1";

   connection db{ioc};
   db.get_config().connect_timeout = std::chrono::milliseconds{1};

   db.async_run(ep, [](auto ec) {
      BOOST_CHECK_EQUAL(ec, aedis::error::connect_timeout);
   });
   ioc.run();
}

#ifdef BOOST_ASIO_HAS_CO_AWAIT

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
      connection::config cfg;
      cfg.resolve_timeout = std::chrono::seconds{1};
      cfg.connect_timeout = std::chrono::seconds{1};
      cfg.ping_interval = std::chrono::seconds{1};

      net::io_context ioc;
      auto db = std::make_shared<connection>(ioc, cfg);

      net::co_spawn(ioc.get_executor(), send_after(db, ms), net::detached);

      endpoint ep{"127.0.0.1", "6379"};
      db->async_run(ep, [](auto ec){
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
      db->get_config().ping_interval = 2 * ms;
      db->get_config().resolve_timeout = 2 * ms;
      db->get_config().connect_timeout = 2 * ms;
      db->get_config().ping_interval = 2 * ms;

      request req;
      req.push("QUIT");

      endpoint ep{"127.0.0.1", "6379"};
      db->async_run(ep, req, adapt(), [](auto ec, auto){
         BOOST_TEST(!ec);
      });

      ioc.run();
   }
}

net::awaitable<void> test_reconnect_impl(std::shared_ptr<connection> db)
{
   request req;
   req.push("QUIT");

   int i = 0;
   endpoint ep{"127.0.0.1", "6379"};
   for (; i < 5; ++i) {
      boost::system::error_code ec;
      co_await db->async_run(ep, req, adapt(), net::redirect_error(net::use_awaitable, ec));
      BOOST_TEST(!ec);
   }

   BOOST_CHECK_EQUAL(i, 5);
   co_return;
}

// Test whether the client works after a reconnect.
BOOST_AUTO_TEST_CASE(test_reconnect)
{
   std::cout << boost::unit_test::framework::current_test_case().p_name << std::endl;
   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc);
   net::co_spawn(ioc, test_reconnect_impl(db), net::detached);
   ioc.run();
}

BOOST_AUTO_TEST_CASE(test_auth_fail)
{
   std::cout << boost::unit_test::framework::current_test_case().p_name << std::endl;
   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc.get_executor());

   // Should cause an error in the authentication as our redis server
   // has no authentication configured.
   endpoint ep;
   ep.host = "127.0.0.1";
   ep.port = "6379";
   ep.username = "caboclo-do-mato";
   ep.password = "jabuticaba";

   db->async_run(ep, [](auto ec) {
      BOOST_CHECK_EQUAL(ec, aedis::error::resp3_simple_error);
   });

   ioc.run();
}

#endif
