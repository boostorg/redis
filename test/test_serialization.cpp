/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/request.hpp>

#include <boost/core/lightweight_test.hpp>

#include <string>
#include <string_view>

using boost::redis::request;

namespace {

// --- Strings ---
void test_string_view()
{
   request req;
   req.push("GET", std::string_view("key"));
   BOOST_TEST_EQ(req.payload(), "*2\r\n$3\r\nGET\r\n$3\r\nkey\r\n");
}

void test_string()
{
   std::string s{"k1"};
   const std::string s2{"k2"};
   request req;
   req.push("GET", s, s2, std::string("k3"));
   BOOST_TEST_EQ(req.payload(), "*4\r\n$3\r\nGET\r\n$2\r\nk1\r\n$2\r\nk2\r\n$2\r\nk3\r\n");
}

void test_c_string()
{
   request req;
   req.push("GET", "k1", static_cast<const char*>("k2"));
   BOOST_TEST_EQ(req.payload(), "*3\r\n$3\r\nGET\r\n$2\r\nk1\r\n$2\r\nk2\r\n");
}

}  // namespace

int main()
{
   test_string_view();
   test_string();
   test_c_string();

   return boost::report_errors();
}
