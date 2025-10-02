//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/connection.hpp>
#include <boost/redis/ignore.hpp>

#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/cancellation_type.hpp>
#include <boost/core/lightweight_test.hpp>

#include "common.hpp"

#include <cstddef>

using boost::system::error_code;
namespace net = boost::asio;
using namespace boost::redis;

namespace {

// Per-operation cancellation works for async_run
void test_terminal_cancellation()
{
   // Setup
   net::io_context ioc;
   connection conn{ioc};
   net::cancellation_signal sig;

   request req;
   req.push("PING", "something");

   bool run_finished = false, exec_finished = false;

   // Run the connection
   auto run_cb = [&](error_code ec) {
      run_finished = true;
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
   };
   conn.async_run(make_test_config(), net::bind_cancellation_slot(sig.slot(), run_cb));

   // Launch a PING
   conn.async_exec(req, ignore, [&](error_code ec, std::size_t) {
      exec_finished = true;
      BOOST_TEST_EQ(ec, error_code());
      sig.emit(net::cancellation_type_t::terminal);
   });

   ioc.run_for(test_timeout);

   // Check
   BOOST_TEST(run_finished);
   BOOST_TEST(exec_finished);
}

}  // namespace

int main()
{
   test_terminal_cancellation();

   return boost::report_errors();
}
