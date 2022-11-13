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

#include "common.hpp"

// TODO: Test whether HELLO won't be inserted passt commands that have
// been already writen.

namespace net = boost::asio;

using aedis::resp3::request;
using aedis::adapt;
using connection = aedis::connection<>;
using error_code = boost::system::error_code;

BOOST_AUTO_TEST_CASE(wrong_response_data_type)
{
   request req;
   req.push("HELLO", 3);
   req.push("QUIT");

   // Wrong data type.
   std::tuple<aedis::ignore, int> resp;
   net::io_context ioc;

   auto const endpoints = resolve();
   connection conn{ioc};
   net::connect(conn.next_layer(), endpoints);

   conn.async_exec(req, adapt(resp), [](auto ec, auto){
      BOOST_CHECK_EQUAL(ec, aedis::error::not_a_number);
   });
   conn.async_run({}, [](auto ec){
      BOOST_CHECK_EQUAL(ec, boost::asio::error::basic_errors::operation_aborted);
   });

   ioc.run();
}

BOOST_AUTO_TEST_CASE(cancel_request_if_not_connected)
{
   request req;
   req.get_config().cancel_if_not_connected = true;
   req.push("HELLO", 3);
   req.push("PING");

   net::io_context ioc;
   auto conn = std::make_shared<connection>(ioc);
   conn->async_exec(req, adapt(), [](auto ec, auto){
      BOOST_CHECK_EQUAL(ec, aedis::error::not_connected);
   });

   ioc.run();
}

BOOST_AUTO_TEST_CASE(request_retry)
{
   request req1;
   req1.get_config().cancel_on_connection_lost = true;
   req1.push("HELLO", 3);
   req1.push("CLIENT", "PAUSE", 7000);

   request req2;
   req2.get_config().cancel_on_connection_lost = false;
   req2.get_config().retry = false;
   req2.push("PING");

   net::io_context ioc;
   auto const endpoints = resolve();
   connection conn{ioc};
   net::connect(conn.next_layer(), endpoints);


   conn.async_exec(req1, adapt(), [](auto ec, auto){
      BOOST_TEST(!ec);
   });

   conn.async_exec(req2, adapt(), [](auto ec, auto){
      BOOST_CHECK_EQUAL(ec, boost::system::errc::errc_t::operation_canceled);
   });

   conn.async_run({}, [](auto ec){
      BOOST_CHECK_EQUAL(ec, aedis::error::idle_timeout);
   });

   ioc.run();
}
