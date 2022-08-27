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
   connection::config cfg;
   cfg.host = "Atibaia";
   cfg.port = "6379";
   cfg.resolve_timeout = std::chrono::seconds{100};

   net::io_context ioc;
   connection db{ioc, cfg};
   db.async_run([](auto ec) {
      BOOST_TEST(is_host_not_found(ec));
   });

   ioc.run();
}

//----------------------------------------------------------------

BOOST_AUTO_TEST_CASE(test_resolve_with_timeout)
{
   net::io_context ioc;

   connection db{ioc};
   db.get_config().host = "Atibaia";
   db.get_config().port = "6379";
   // Low-enough to cause a timeout always.
   db.get_config().resolve_timeout = std::chrono::milliseconds{1};

   db.async_run([](auto const& ec) {
      BOOST_CHECK_EQUAL(ec, aedis::error::resolve_timeout);
   });

   ioc.run();
}

//----------------------------------------------------------------

BOOST_AUTO_TEST_CASE(test_connect)
{
   connection::config cfg;
   cfg.host = "127.0.0.1";
   cfg.port = "1";
   cfg.connect_timeout = std::chrono::seconds{100};

   net::io_context ioc;
   connection db{ioc, cfg};
   db.async_run([](auto ec) {
      BOOST_CHECK_EQUAL(ec, net::error::basic_errors::connection_refused);
   });
   ioc.run();
}

//----------------------------------------------------------------

BOOST_AUTO_TEST_CASE(test_connect_timeout)
{
   net::io_context ioc;
   connection db{ioc};
   db.get_config().host = "example.com";
   db.get_config().port = "1";
   db.get_config().connect_timeout = std::chrono::milliseconds{1};

   db.async_run([](auto ec) {
      BOOST_CHECK_EQUAL(ec, aedis::error::connect_timeout);
   });
   ioc.run();
}

//----------------------------------------------------------------

// Test if quit causes async_run to exit.
void test_quit1(connection::config const& cfg)
{
   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc, cfg);

   request req;
   req.push("QUIT");

   db->async_exec(req, adapt(), [](auto ec, auto){
      BOOST_TEST(!ec);
   });

   db->async_run([](auto ec){
      BOOST_CHECK_EQUAL(ec, net::error::misc_errors::eof);
   });

   ioc.run();
}

void test_quit2(connection::config const& cfg)
{
   std::cout << "test_quit2" << std::endl;
   request req;
   req.push("QUIT");

   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc, cfg);
   db->async_run(req, adapt(), [](auto ec, auto){
      BOOST_TEST(!ec);
   });

   ioc.run();
}

BOOST_AUTO_TEST_CASE(test_quit)
{
   connection::config cfg;

   cfg.coalesce_requests = true;
   test_quit1(cfg);

   cfg.coalesce_requests = false;
   test_quit1(cfg);

   cfg.coalesce_requests = true;
   test_quit2(cfg);

   cfg.coalesce_requests = false;
   test_quit2(cfg);
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

      db->async_run([](auto ec){
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

      db->async_run(req, adapt(), [](auto ec, auto){
         BOOST_TEST(!ec);
      });

      ioc.run();
   }
}

net::awaitable<void> test_reconnect_impl(std::shared_ptr<connection> db)
{
   request req;
   req.push("QUIT");

   for (auto i = 0;;) {
      auto ev = co_await db->async_receive_event(net::use_awaitable);
      auto const r1 = ev == connection::event::resolve;
      BOOST_TEST(r1);

      ev = co_await db->async_receive_event(net::use_awaitable);
      auto const r2 = ev == connection::event::connect;
      BOOST_TEST(r2);

      ev = co_await db->async_receive_event(net::use_awaitable);
      auto const r3 = ev == connection::event::hello;
      BOOST_TEST(r3);

      co_await db->async_exec(req, adapt(), net::use_awaitable);

      // Test 5 reconnetions and returns.

      ++i;
      if (i == 5) {
         db->get_config().enable_reconnect = false;
         co_return;
      }
   }

   co_return;
}

// Test whether the client works after a reconnect.
BOOST_AUTO_TEST_CASE(test_reconnect)
{
   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc.get_executor());
   db->get_config().enable_events = true;
   db->get_config().enable_reconnect = true;
   db->get_config().reconnect_interval = std::chrono::milliseconds{100};

   net::co_spawn(ioc, test_reconnect_impl(db), net::detached);

   db->async_run([](auto ec) {
      BOOST_CHECK_EQUAL(ec, net::error::misc_errors::eof);
   });

   ioc.run();
}

BOOST_AUTO_TEST_CASE(test_auth_fail)
{
   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc.get_executor());

   // Should cause an error in the authentication as our redis server
   // has no authentication configured.
   db->get_config().username = "caboclo-do-mato";
   db->get_config().password = "jabuticaba";

   db->async_run([](auto ec) {
      BOOST_CHECK_EQUAL(ec, aedis::error::resp3_simple_error);
   });

   ioc.run();
}

#endif
