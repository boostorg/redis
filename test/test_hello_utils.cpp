//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/detail/hello_utils.hpp>

#include <boost/core/lightweight_test.hpp>

#include "boost/redis/config.hpp"
#include "boost/redis/request.hpp"

using boost::redis::request;
using boost::redis::config;
using boost::redis::detail::setup_hello_request;

namespace {

void test_setup_hello_request()
{
   config cfg;
   cfg.clientname = "";
   request req;

   setup_hello_request(cfg, req);

   std::string_view const expected = "*2\r\n$5\r\nHELLO\r\n$1\r\n3\r\n";
   BOOST_TEST_EQ(req.payload(), expected);
}

void test_setup_hello_request_select()
{
   config cfg;
   cfg.clientname = "";
   cfg.database_index = 10;
   request req;

   setup_hello_request(cfg, req);

   std::string_view const expected =
      "*2\r\n$5\r\nHELLO\r\n$1\r\n3\r\n"
      "*2\r\n$6\r\nSELECT\r\n$2\r\n10\r\n";
   BOOST_TEST_EQ(req.payload(), expected);
}

void test_setup_hello_request_clientname()
{
   config cfg;
   request req;

   setup_hello_request(cfg, req);

   std::string_view const
      expected = "*4\r\n$5\r\nHELLO\r\n$1\r\n3\r\n$7\r\nSETNAME\r\n$11\r\nBoost.Redis\r\n";
   BOOST_TEST_EQ(req.payload(), expected);
}

void test_setup_hello_request_auth()
{
   config cfg;
   cfg.clientname = "";
   cfg.username = "foo";
   cfg.password = "bar";
   request req;

   setup_hello_request(cfg, req);

   std::string_view const
      expected = "*5\r\n$5\r\nHELLO\r\n$1\r\n3\r\n$4\r\nAUTH\r\n$3\r\nfoo\r\n$3\r\nbar\r\n";
   BOOST_TEST_EQ(req.payload(), expected);
}

}  // namespace

int main()
{
   test_setup_hello_request();
   test_setup_hello_request_select();
   test_setup_hello_request_clientname();
   test_setup_hello_request_auth();

   return boost::report_errors();
}