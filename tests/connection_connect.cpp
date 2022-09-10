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

