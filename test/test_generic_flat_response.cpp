/* Copyright (c) 2018-2026 Marcelo Zimbres Silva (mzimbres@gmail.com),
 * Ruben Perez Hidalgo (rubenperez038@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/adapter/adapt.hpp>
#include <boost/redis/resp3/serialization.hpp>
#include <boost/redis/response.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/system/error_code.hpp>

using namespace boost::redis;
using boost::system::error_code;
using boost::redis::resp3::detail::deserialize;
using adapter::adapt2;

void test_simple_error()
{
   generic_flat_response resp;

   char const* wire = "-Error\r\n";

   error_code ec;
   deserialize(wire, adapt2(resp), ec);
   BOOST_TEST_EQ(ec, error_code{});

   BOOST_TEST(!resp.has_value());
   BOOST_TEST(resp.has_error());
   auto const error = resp.error();

   BOOST_TEST_EQ(error.data_type, boost::redis::resp3::type::simple_error);
   BOOST_TEST_EQ(error.diagnostic, std::string{"Error"});
}

void test_blob_error()
{
   generic_flat_response resp;

   char const* wire = "!5\r\nError\r\n";

   error_code ec;
   deserialize(wire, adapt2(resp), ec);
   BOOST_TEST_EQ(ec, error_code{});

   BOOST_TEST(!resp.has_value());
   BOOST_TEST(resp.has_error());
   auto const error = resp.error();

   BOOST_TEST_EQ(error.data_type, boost::redis::resp3::type::blob_error);
   BOOST_TEST_EQ(error.diagnostic, std::string{"Error"});
}

int main()
{
   test_simple_error();
   test_blob_error();

   return boost::report_errors();
}
