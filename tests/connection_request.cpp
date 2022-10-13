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

using aedis::resp3::request;
using aedis::adapt;
using connection = aedis::connection<>;
using endpoint = aedis::endpoint;
using error_code = boost::system::error_code;

#ifdef BOOST_ASIO_HAS_CO_AWAIT
#include <boost/asio/experimental/awaitable_operators.hpp>
using namespace net::experimental::awaitable_operators;
#endif

BOOST_AUTO_TEST_CASE(wrong_response_data_type)
{
   std::cout << boost::unit_test::framework::current_test_case().p_name << std::endl;
   request req;
   req.push("QUIT");

   // Wrong data type.
   std::tuple<int> resp;
   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc);
   db->async_exec(req, adapt(resp), [](auto ec, auto){
      BOOST_CHECK_EQUAL(ec, aedis::error::not_a_number);
   });
   db->async_run({"127.0.0.1", "6379"}, {}, [](auto ec, auto){
      BOOST_CHECK_EQUAL(ec, boost::system::errc::errc_t::operation_canceled);
   });

   ioc.run();
}

BOOST_AUTO_TEST_CASE(cancel_request_if_not_connected)
{
   std::cout << boost::unit_test::framework::current_test_case().p_name << std::endl;
   request req;
   req.get_config().cancel_if_not_connected = true;
   req.push("PING");

   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc);
   db->async_exec(req, adapt(), [](auto ec, auto){
      BOOST_CHECK_EQUAL(ec, aedis::error::not_connected);
   });

   ioc.run();
}

BOOST_AUTO_TEST_CASE(request_retry)
{
   std::cout << boost::unit_test::framework::current_test_case().p_name << std::endl;

   request req1;
   req1.get_config().cancel_on_connection_lost = true;
   req1.push("CLIENT", "PAUSE", 7000);

   request req2;
   req2.get_config().cancel_on_connection_lost = false;
   req2.get_config().retry = false;
   req2.push("PING");

   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc);

   db->async_exec(req1, adapt(), [](auto ec, auto){
      BOOST_TEST(!ec);
   });

   db->async_exec(req2, adapt(), [](auto ec, auto){
      BOOST_CHECK_EQUAL(ec, boost::system::errc::errc_t::operation_canceled);
   });

   db->async_run({"127.0.0.1", "6379"}, {}, [](auto ec, auto){
      BOOST_CHECK_EQUAL(ec, aedis::error::idle_timeout);
   });

   ioc.run();
}
