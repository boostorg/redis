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

using connection = aedis::connection<>;
using endpoint = aedis::endpoint;
using error_code = boost::system::error_code;

bool is_host_not_found(error_code ec)
{
   if (ec == net::error::netdb_errors::host_not_found) return true;
   if (ec == net::error::netdb_errors::host_not_found_try_again) return true;
   return false;
}

error_code test_async_run(endpoint ep, connection::config cfg = {})
{
   net::io_context ioc;
   connection db{ioc, cfg};
   error_code ret;
   db.async_run(ep, [&](auto ec) { ret = ec; });
   ioc.run();
   return ret;
}

// Tests whether resolve fails with the correct error.
BOOST_AUTO_TEST_CASE(test_resolve)
{
   std::cout << boost::unit_test::framework::current_test_case().p_name << std::endl;

   endpoint ep;
   ep.host = "Atibaia";
   ep.port = "6379";

   connection::config cfg;
   cfg.resolve_timeout = std::chrono::seconds{100};
   auto const ec = test_async_run(ep, cfg);

   BOOST_TEST(is_host_not_found(ec));
}

BOOST_AUTO_TEST_CASE(test_resolve_with_timeout)
{
   std::cout << boost::unit_test::framework::current_test_case().p_name << std::endl;

   endpoint ep;
   ep.host = "Atibaia";
   ep.port = "6379";

   connection::config cfg;
   // Low-enough to cause a timeout always.
   cfg.resolve_timeout = std::chrono::milliseconds{1};
   auto const ec = test_async_run(ep, cfg);
   BOOST_CHECK_EQUAL(ec, aedis::error::resolve_timeout);
}

BOOST_AUTO_TEST_CASE(test_connect)
{
   std::cout << boost::unit_test::framework::current_test_case().p_name << std::endl;

   endpoint ep;
   ep.host = "127.0.0.1";
   ep.port = "1";

   connection::config cfg;
   cfg.connect_timeout = std::chrono::seconds{100};
   auto const ec = test_async_run(ep, cfg);
   BOOST_CHECK_EQUAL(ec, net::error::basic_errors::connection_refused);
}

BOOST_AUTO_TEST_CASE(test_connect_timeout)
{
   std::cout << boost::unit_test::framework::current_test_case().p_name << std::endl;

   endpoint ep;
   ep.host = "example.com";
   ep.port = "1";

   connection::config cfg;
   cfg.connect_timeout = std::chrono::milliseconds{1};
   auto const ec = test_async_run(ep, cfg);
   BOOST_CHECK_EQUAL(ec, aedis::error::connect_timeout);
}

BOOST_AUTO_TEST_CASE(test_hello_fail)
{
   std::cout << boost::unit_test::framework::current_test_case().p_name << std::endl;

   // Succeeds with the tcp connection but fails the hello.
   endpoint ep;
   ep.host = "google.com";
   ep.port = "80";

   auto const ec = test_async_run(ep);
   BOOST_CHECK_EQUAL(ec, aedis::error::invalid_data_type);
}

BOOST_AUTO_TEST_CASE(test_hello_tls_over_plain_fail)
{
   std::cout << boost::unit_test::framework::current_test_case().p_name << std::endl;

   endpoint ep;
   ep.host = "google.com";
   ep.port = "443";

   auto const ec = test_async_run(ep);
   BOOST_CHECK_EQUAL(ec, net::error::misc_errors::eof);
}

BOOST_AUTO_TEST_CASE(test_auth_fail)
{
   std::cout << boost::unit_test::framework::current_test_case().p_name << std::endl;

   // Should cause an error in the authentication as our redis server
   // has no authentication configured.
   endpoint ep;
   ep.host = "127.0.0.1";
   ep.port = "6379";
   ep.username = "caboclo-do-mato";
   ep.password = "jabuticaba";

   auto const ec = test_async_run(ep);
   BOOST_CHECK_EQUAL(ec, aedis::error::resp3_simple_error);
}

BOOST_AUTO_TEST_CASE(test_wrong_role)
{
   std::cout << boost::unit_test::framework::current_test_case().p_name << std::endl;

   // Should cause an error in the authentication as our redis server
   // has no authentication configured.
   endpoint ep;
   ep.host = "127.0.0.1";
   ep.port = "6379";
   ep.role = "errado";

   auto const ec = test_async_run(ep);
   BOOST_CHECK_EQUAL(ec, aedis::error::unexpected_server_role);
}
