//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/connection.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/response.hpp>

#include <boost/core/lightweight_test.hpp>

#include "boost/asio/cancel_after.hpp"
#include "boost/asio/error.hpp"
#include "boost/asio/io_context.hpp"
#include "common.hpp"

using namespace std::chrono_literals;
namespace asio = boost::asio;
using boost::system::error_code;
using boost::redis::request;
using boost::redis::basic_connection;
using boost::redis::connection;

namespace {

void test_basic_connection()
{
   // Setup
   asio::io_context ioc;
   basic_connection<asio::io_context::executor_type> conn{ioc};
   bool run_finished = false;

   // async_run
   conn.async_run(make_test_config(), asio::cancel_after(1ms, [&](error_code ec) {
                     BOOST_TEST_EQ(ec, asio::error::operation_aborted);
                     run_finished = true;
                  }));

   ioc.run_for(test_timeout);

   BOOST_TEST(run_finished);
}

}  // namespace

int main()
{
   test_basic_connection();

   return boost::report_errors();
}
