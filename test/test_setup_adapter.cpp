//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/detail/connection_state.hpp>
#include <boost/redis/impl/setup_request_utils.hpp>
#include <boost/redis/resp3/parser.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/system/detail/error_code.hpp>
#include <boost/system/result.hpp>

using namespace boost::redis;
using detail::setup_adapter;
using detail::connection_state;
using boost::system::error_code;

namespace {

void test_success()
{
   // Setup
   connection_state st;
   st.cfg.setup.push("SELECT", 2);
   setup_adapter adapter{st};

   // Response to HELLO
   resp3::parser p;
   error_code ec;
   bool done = resp3::parse(p, "%1\r\n$6\r\nserver\r\n$5\r\nredis\r\n", adapter, ec);
   BOOST_TEST(done);
   BOOST_TEST_EQ(ec, error_code());

   // Response to the SELECT command
   p.reset();
   done = resp3::parse(p, "+OK\r\n", adapter, ec);
   BOOST_TEST(done);
   BOOST_TEST_EQ(ec, error_code());

   // No diagnostic
   BOOST_TEST_EQ(st.setup_diagnostic, "");
}

}  // namespace

int main()
{
   test_success();

   return boost::report_errors();
}