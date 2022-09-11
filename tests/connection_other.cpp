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
         BOOST_CHECK_EQUAL(ec, aedis::error::exec_timeout);
      });

      ioc.run();
   }
}

#endif

BOOST_AUTO_TEST_CASE(test_wrong_data_type)
{
   request req;
   req.push("QUIT");

   // Wrong data type.
   std::tuple<int> resp;
   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc);
   endpoint ep{"127.0.0.1", "6379"};
   db->async_run(ep, req, adapt(resp), [](auto ec, auto){
      BOOST_CHECK_EQUAL(ec, aedis::error::not_a_number);
   });

   ioc.run();
}
