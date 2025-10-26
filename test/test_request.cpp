/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/request.hpp>

#include <boost/core/lightweight_test.hpp>

#include <map>
#include <string>
#include <string_view>

using boost::redis::request;

// TODO: Serialization.

namespace {

void test_push_no_args()
{
   request req1;
   req1.push("PING");
   BOOST_TEST_EQ(req1.payload(), "*1\r\n$4\r\nPING\r\n");
}

void test_push_int()
{
   request req;
   req.push("PING", 42);
   BOOST_TEST_EQ(req.payload(), "*2\r\n$4\r\nPING\r\n$2\r\n42\r\n");
}

void test_push_multiple_args()
{
   char const* res = "*5\r\n$3\r\nSET\r\n$3\r\nkey\r\n$5\r\nvalue\r\n$2\r\nEX\r\n$1\r\n2\r\n";
   request req;
   req.push("SET", "key", "value", "EX", "2");
   BOOST_TEST_EQ(req.payload(), res);
}

void test_push_range()
{
   std::map<std::string, std::string> in{
      {"key1", "value1"},
      {"key2", "value2"}
   };

   constexpr std::string_view expected =
      "*6\r\n$4\r\nHSET\r\n$3\r\nkey\r\n$4\r\nkey1\r\n$6\r\nvalue1\r\n$4\r\nkey2\r\n$"
      "6\r\nvalue2\r\n";

   request req1;
   req1.push_range("HSET", "key", in);
   BOOST_TEST_EQ(req1.payload(), expected);

   request req2;
   req2.push_range("HSET", "key", std::cbegin(in), std::cend(in));
   BOOST_TEST_EQ(req2.payload(), expected);
}

}  // namespace

int main()
{
   test_push_no_args();
   test_push_int();
   test_push_multiple_args();
   test_push_range();

   return boost::report_errors();
}
