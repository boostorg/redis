/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <iostream>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#define BOOST_TEST_MODULE low level
#include <boost/test/included/unit_test.hpp>

#include <aedis.hpp>
#include <aedis/ssl/connection.hpp>
#include <aedis/src.hpp>

namespace net = boost::asio;

using aedis::adapt;
using connection = aedis::ssl::connection<net::ssl::stream<net::ip::tcp::socket>>;
using endpoint = aedis::endpoint;

bool verify_certificate(bool, net::ssl::verify_context&)
{
   std::cout << "set_verify_callback" << std::endl;
   return true;
}

boost::system::error_code hello_fail(endpoint ep)
{
   net::io_context ioc;

   net::ssl::context ctx{net::ssl::context::sslv23};
   auto conn = std::make_shared<connection>(ioc.get_executor(), ctx);
   conn->next_layer().set_verify_mode(net::ssl::verify_peer);
   conn->next_layer().set_verify_callback(verify_certificate);
   boost::system::error_code ret;
   conn->async_run(ep, [&](auto ec) {
      ret = ec;
   });

   ioc.run();
   return ret;
}

BOOST_AUTO_TEST_CASE(test_tls_handshake_fail)
{
   std::cout << boost::unit_test::framework::current_test_case().p_name << std::endl;

   endpoint ep;
   ep.host = "google.com";
   ep.port = "80";
   auto const ec = hello_fail(ep);
   BOOST_TEST(!!ec);
   std::cout << "-----> " << ec.message() << std::endl;
}

BOOST_AUTO_TEST_CASE(test_tls_handshake_fail2)
{
   std::cout << boost::unit_test::framework::current_test_case().p_name << std::endl;

   endpoint ep;
   ep.host = "127.0.0.1";
   ep.port = "6379";
   auto const ec = hello_fail(ep);
   BOOST_CHECK_EQUAL(ec, aedis::error::ssl_handshake_timeout);
}

BOOST_AUTO_TEST_CASE(test_hello_fail)
{
   std::cout << boost::unit_test::framework::current_test_case().p_name << std::endl;

   endpoint ep;
   ep.host = "google.com";
   ep.port = "443";
   auto const ec = hello_fail(ep);
   BOOST_CHECK_EQUAL(ec, aedis::error::invalid_data_type);
}

