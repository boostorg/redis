/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/detail/runner.hpp>
#include <boost/redis/resp3/serialization.hpp>
#include <boost/redis/adapter/adapt.hpp>
#define BOOST_TEST_MODULE conn-quit
#include <boost/test/included/unit_test.hpp>
#include <string>
#include <iostream>

using boost::redis::request;
using boost::redis::config;
using boost::redis::detail::push_hello;
using boost::redis::adapter::adapt2;
using boost::redis::adapter::result;
using boost::redis::resp3::detail::deserialize;

BOOST_AUTO_TEST_CASE(low_level_sync_sans_io)
{
   try {
      result<std::set<std::string>> resp;

      char const* wire = "~6\r\n+orange\r\n+apple\r\n+one\r\n+two\r\n+three\r\n+orange\r\n";
      deserialize(wire, adapt2(resp));

      for (auto const& e: resp.value())
         std::cout << e << std::endl;

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
      exit(EXIT_FAILURE);
   }
}

BOOST_AUTO_TEST_CASE(config_to_hello)
{
   config cfg;
   cfg.clientname = "";
   request req;

   push_hello(cfg, req);

   std::string_view const expected = "*2\r\n$5\r\nHELLO\r\n$1\r\n3\r\n";
   BOOST_CHECK_EQUAL(req.payload(), expected);
}

BOOST_AUTO_TEST_CASE(config_to_hello_with_select)
{
   config cfg;
   cfg.clientname = "";
   cfg.database_index = 10;
   request req;

   push_hello(cfg, req);

   std::string_view const expected =
      "*2\r\n$5\r\nHELLO\r\n$1\r\n3\r\n"
      "*2\r\n$6\r\nSELECT\r\n$2\r\n10\r\n";

   BOOST_CHECK_EQUAL(req.payload(), expected);
}

BOOST_AUTO_TEST_CASE(config_to_hello_cmd_clientname)
{
   config cfg;
   request req;

   push_hello(cfg, req);

   std::string_view const expected = "*4\r\n$5\r\nHELLO\r\n$1\r\n3\r\n$7\r\nSETNAME\r\n$11\r\nBoost.Redis\r\n";
   BOOST_CHECK_EQUAL(req.payload(), expected);
}

BOOST_AUTO_TEST_CASE(config_to_hello_cmd_auth)
{
   config cfg;
   cfg.clientname = "";
   cfg.username = "foo";
   cfg.password = "bar";
   request req;

   push_hello(cfg, req);

   std::string_view const expected = "*5\r\n$5\r\nHELLO\r\n$1\r\n3\r\n$4\r\nAUTH\r\n$3\r\nfoo\r\n$3\r\nbar\r\n";
   BOOST_CHECK_EQUAL(req.payload(), expected);
}
