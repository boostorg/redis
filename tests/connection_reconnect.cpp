/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <iostream>
#include <boost/asio.hpp>

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

#ifdef BOOST_ASIO_HAS_CO_AWAIT

net::awaitable<void> test_reconnect_impl(std::shared_ptr<connection> db)
{
   request req;
   req.push("QUIT");

   int i = 0;
   endpoint ep{"127.0.0.1", "6379"};
   for (; i < 5; ++i) {
      boost::system::error_code ec;
      co_await db->async_run(ep, req, adapt(), net::redirect_error(net::use_awaitable, ec));
      db->reset_stream();
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

#endif

BOOST_AUTO_TEST_CASE(test_reconnect_timeout)
{
   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc);

   request req1;
   req1.push("CLIENT", "PAUSE", 7000);

   request req2;
   req2.push("QUIT");

   endpoint ep{"127.0.0.1", "6379"};
   db->async_run(ep, req1, adapt(), [db, &req2, &ep](auto ec, auto){
      BOOST_CHECK_EQUAL(ec, aedis::error::idle_timeout);
      db->reset_stream();
      db->async_run(ep, req2, adapt(), [db](auto ec, auto){
         BOOST_CHECK_EQUAL(ec, aedis::error::exec_timeout);
      });
   });

   ioc.run();
}
