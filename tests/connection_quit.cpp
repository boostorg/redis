/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <iostream>
#include <boost/asio.hpp>
#include <boost/system/errc.hpp>

#define BOOST_TEST_MODULE low level
#include <boost/test/included/unit_test.hpp>

#include <aedis.hpp>
#include <aedis/src.hpp>

namespace net = boost::asio;

using aedis::adapt;
using aedis::endpoint;
using aedis::resp3::request;
using connection = aedis::connection<>;
using error_code = boost::system::error_code;

// Test if quit causes async_run to exit.
BOOST_AUTO_TEST_CASE(test_quit_no_coalesce)
{
   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc);

   request req1{{false, false}};
   req1.push("PING");

   request req2{{false, false}};
   req2.push("QUIT");

   db->async_exec(req1, adapt(), [](auto ec, auto){ BOOST_TEST(!ec); });
   db->async_exec(req2, adapt(), [](auto ec, auto){ BOOST_TEST(!ec); });
   db->async_exec(req1, adapt(), [](auto ec, auto){
      BOOST_CHECK_EQUAL(ec, boost::system::errc::errc_t::operation_canceled);
   });
   db->async_exec(req1, adapt(), [](auto ec, auto){
         BOOST_CHECK_EQUAL(ec, boost::system::errc::errc_t::operation_canceled);
   });
   db->async_exec(req1, adapt(), [](auto ec, auto){
      BOOST_CHECK_EQUAL(ec, boost::system::errc::errc_t::operation_canceled);
   });

   endpoint ep{"127.0.0.1", "6379"};
   db->async_run(ep, [db](auto ec){
      BOOST_CHECK_EQUAL(ec, net::error::misc_errors::eof);
      db->cancel(connection::operation::exec);
   });

   ioc.run();
}

BOOST_AUTO_TEST_CASE(test_quit_coalesce)
{
   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc);

   request req1;
   req1.push("PING");

   request req2;
   req2.push("QUIT");

   db->async_exec(req1, adapt(), [](auto ec, auto){
      BOOST_TEST(!ec);
   });
   db->async_exec(req2, adapt(), [](auto ec, auto){
      BOOST_TEST(!ec);
   });
   db->async_exec(req1, adapt(), [](auto ec, auto){
      BOOST_CHECK_EQUAL(ec, net::error::misc_errors::eof);
   });
   db->async_exec(req1, adapt(), [](auto ec, auto){
      BOOST_CHECK_EQUAL(ec, boost::system::errc::errc_t::operation_canceled);
   });

   endpoint ep{"127.0.0.1", "6379"};
   db->async_run(ep, [db](auto ec){
      BOOST_CHECK_EQUAL(ec, boost::system::errc::errc_t::operation_canceled);
      db->cancel(connection::operation::exec);
   });

   ioc.run();
}

void test_quit2(bool coalesce)
{
   request req{{false, coalesce}};
   req.push("QUIT");

   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc);
   endpoint ep{"127.0.0.1", "6379"};
   db->async_run(ep, req, adapt(), [](auto ec, auto) {
      BOOST_CHECK_EQUAL(ec, net::error::misc_errors::eof);
   });

   ioc.run();
}

BOOST_AUTO_TEST_CASE(test_quit)
{
   std::cout << boost::unit_test::framework::current_test_case().p_name << std::endl;
   test_quit2(true);
   test_quit2(false);
}
