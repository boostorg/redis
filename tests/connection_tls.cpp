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

using aedis::adapt;
using connection = aedis::connection<>;
using endpoint = aedis::endpoint;

BOOST_AUTO_TEST_CASE(test_hello_fail)
{
   std::cout << boost::unit_test::framework::current_test_case().p_name << std::endl;

   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc.get_executor());

   // Succeeds with the tcp connection but fails the hello.
   endpoint ep;
   ep.host = "google.com";
   ep.port = "443";

   db->async_run(ep, [](auto ec) {
      BOOST_CHECK_EQUAL(ec, net::error::misc_errors::eof);
   });

   ioc.run();
}

