//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/adapter/result.hpp>
#include <boost/redis/config.hpp>
#include <boost/redis/detail/hello_utils.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/resp3/type.hpp>
#include <boost/redis/response.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/system/result.hpp>

namespace redis = boost::redis;
using redis::detail::setup_hello_request;
using redis::detail::clear_response;

namespace {

void test_setup_hello_request()
{
   redis::config cfg;
   cfg.clientname = "";
   redis::request req;

   setup_hello_request(cfg, req);

   std::string_view const expected = "*2\r\n$5\r\nHELLO\r\n$1\r\n3\r\n";
   BOOST_TEST_EQ(req.payload(), expected);
}

void test_setup_hello_request_select()
{
   redis::config cfg;
   cfg.clientname = "";
   cfg.database_index = 10;
   redis::request req;

   setup_hello_request(cfg, req);

   std::string_view const expected =
      "*2\r\n$5\r\nHELLO\r\n$1\r\n3\r\n"
      "*2\r\n$6\r\nSELECT\r\n$2\r\n10\r\n";
   BOOST_TEST_EQ(req.payload(), expected);
}

void test_setup_hello_request_clientname()
{
   redis::config cfg;
   redis::request req;

   setup_hello_request(cfg, req);

   std::string_view const
      expected = "*4\r\n$5\r\nHELLO\r\n$1\r\n3\r\n$7\r\nSETNAME\r\n$11\r\nBoost.Redis\r\n";
   BOOST_TEST_EQ(req.payload(), expected);
}

void test_setup_hello_request_auth()
{
   redis::config cfg;
   cfg.clientname = "";
   cfg.username = "foo";
   cfg.password = "bar";
   redis::request req;

   setup_hello_request(cfg, req);

   std::string_view const
      expected = "*5\r\n$5\r\nHELLO\r\n$1\r\n3\r\n$4\r\nAUTH\r\n$3\r\nfoo\r\n$3\r\nbar\r\n";
   BOOST_TEST_EQ(req.payload(), expected);
}

void test_setup_hello_request_auth_empty_password()
{
   redis::config cfg;
   cfg.clientname = "";
   cfg.username = "foo";
   redis::request req;

   setup_hello_request(cfg, req);

   std::string_view const
      expected = "*5\r\n$5\r\nHELLO\r\n$1\r\n3\r\n$4\r\nAUTH\r\n$3\r\nfoo\r\n$0\r\n\r\n";
   BOOST_TEST_EQ(req.payload(), expected);
}

void test_setup_hello_request_auth_setname()
{
   redis::config cfg;
   cfg.clientname = "mytest";
   cfg.username = "foo";
   cfg.password = "bar";
   redis::request req;

   setup_hello_request(cfg, req);

   std::string_view const expected =
      "*7\r\n$5\r\nHELLO\r\n$1\r\n3\r\n$4\r\nAUTH\r\n$3\r\nfoo\r\n$3\r\nbar\r\n$7\r\nSETNAME\r\n$"
      "6\r\nmytest\r\n";
   BOOST_TEST_EQ(req.payload(), expected);
}

// clear response
void test_clear_response_empty()
{
   redis::generic_response resp;
   clear_response(resp);
   BOOST_TEST(resp.has_value());
   BOOST_TEST_EQ(resp.value().size(), 0u);
}

void test_clear_response_nonempty()
{
   redis::generic_response resp;
   resp->push_back({});
   clear_response(resp);
   BOOST_TEST(resp.has_value());
   BOOST_TEST_EQ(resp.value().size(), 0u);
}

void test_clear_response_error()
{
   redis::generic_response resp{
      boost::system::in_place_error,
      redis::adapter::error{redis::resp3::type::blob_error, "message"}
   };
   clear_response(resp);
   BOOST_TEST(resp.has_value());
   BOOST_TEST_EQ(resp.value().size(), 0u);
}

}  // namespace

int main()
{
   test_setup_hello_request();
   test_setup_hello_request_select();
   test_setup_hello_request_clientname();
   test_setup_hello_request_auth();
   test_setup_hello_request_auth_empty_password();
   test_setup_hello_request_auth_setname();

   test_clear_response_empty();
   test_clear_response_nonempty();
   test_clear_response_error();

   return boost::report_errors();
}