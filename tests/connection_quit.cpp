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
using error_code = boost::system::error_code;
using net::experimental::as_tuple;

bool is_host_not_found(boost::system::error_code ec)
{
   if (ec == net::error::netdb_errors::host_not_found) return true;
   if (ec == net::error::netdb_errors::host_not_found_try_again) return true;
   return false;
}

// Test if quit causes async_run to exit.
BOOST_AUTO_TEST_CASE(test_quit_no_coalesce)
{
   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc);
   db->get_config().coalesce_requests = false;

   request req1;
   req1.push("PING");

   request req2;
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

   db->async_run([db](auto ec){
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

   db->async_run([db](auto ec){
      BOOST_CHECK_EQUAL(ec, boost::system::errc::errc_t::operation_canceled);
      db->cancel(connection::operation::exec);
   });

   ioc.run();
}

void test_quit2(connection::config const& cfg)
{
   request req;
   req.push("QUIT");

   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc, cfg);
   db->async_run(req, adapt(), [](auto ec, auto){ BOOST_TEST(!ec); });

   ioc.run();
}

BOOST_AUTO_TEST_CASE(test_quit)
{
   std::cout << boost::unit_test::framework::current_test_case().p_name << std::endl;
   connection::config cfg;

   cfg.coalesce_requests = true;
   test_quit2(cfg);

   cfg.coalesce_requests = false;
   test_quit2(cfg);
}

BOOST_AUTO_TEST_CASE(test_wrong_data_type)
{
   request req;
   req.push("QUIT");

   // Wrong data type.
   std::tuple<int> resp;
   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc);
   db->async_run(req, adapt(resp), [](auto ec, auto){
      BOOST_CHECK_EQUAL(ec, aedis::error::not_a_number);
   });

   ioc.run();
}

BOOST_AUTO_TEST_CASE(test_reconnect_timeout)
{
   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc);
   db->get_config().enable_reconnect = true;

   request req1;
   req1.push("CLIENT", "PAUSE", 7000);

   request req2;
   req2.push("QUIT");

   db->async_exec(req1, adapt(), [db, &req2](auto ec, auto){
      BOOST_TEST(!ec);
      db->async_exec(req2, adapt(), [db](auto ec, auto){
         db->get_config().enable_reconnect = false;
         BOOST_TEST(!ec);
      });
   });

   db->async_run([](auto ec){
      BOOST_CHECK_EQUAL(ec, net::error::misc_errors::eof);
   });

   ioc.run();
}
