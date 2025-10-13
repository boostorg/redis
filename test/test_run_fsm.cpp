//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/config.hpp>
#include <boost/redis/detail/connection_state.hpp>
#include <boost/redis/detail/run_fsm.hpp>
#include <boost/redis/error.hpp>

#include <boost/asio/error.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/system/error_code.hpp>

#include "sansio_utils.hpp"

#include <ostream>

using namespace boost::redis;
namespace asio = boost::asio;
using detail::run_fsm;
using detail::multiplexer;
using detail::run_action_type;
using detail::run_action;
using boost::system::error_code;
using boost::asio::cancellation_type_t;
using detail::connection_logger;
using namespace std::chrono_literals;

// Operators
static const char* to_string(run_action_type value)
{
   switch (value) {
      case run_action_type::done:                  return "run_action_type::done";
      case run_action_type::immediate:             return "run_action_type::immediate";
      case run_action_type::connect:               return "run_action_type::connect";
      case run_action_type::parallel_group:        return "run_action_type::parallel_group";
      case run_action_type::cancel_receive:        return "run_action_type::cancel_receive";
      case run_action_type::wait_for_reconnection: return "run_action_type::wait_for_reconnection";
      default:                                     return "<unknown run_action_type>";
   }
}

namespace boost::redis::detail {

std::ostream& operator<<(std::ostream& os, run_action_type type)
{
   os << to_string(type);
   return os;
}

bool operator==(const run_action& lhs, const run_action& rhs) noexcept
{
   return lhs.type == rhs.type && lhs.ec == rhs.ec;
}

std::ostream& operator<<(std::ostream& os, const run_action& act)
{
   os << "run_action{ .type=" << act.type;
   if (act.type == run_action_type::done)
      os << ", .error=" << act.ec;
   return os << " }";
}

}  // namespace boost::redis::detail

namespace {

struct fixture : detail::log_fixture {
   detail::connection_state st;
   run_fsm fsm;

   fixture(config&& cfg = {})
   : st{make_logger(), std::move(cfg)}
   { }
};

config config_no_reconnect()
{
   config res;
   res.reconnect_wait_interval = 0s;
   return res;
}

// The connection is run successfully, then cancelled
void test_success()
{
   // Setup
   fixture fix;

   // Run the operation. We connect and launch the tasks
   auto act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::connect);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::parallel_group);

   // This exits because the operation gets cancelled. Any receive operation gets cancelled
   act = fix.fsm.resume(fix.st, asio::error::operation_aborted, cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, run_action_type::cancel_receive);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, error_code(asio::error::operation_aborted));
}

// The connection is run successfully but with reconnection disabled
// TODO: does this test add anything?
void test_success_no_reconnect()
{
   // Setup
   fixture fix{config_no_reconnect()};

   // Run the operation. We connect and launch the tasks
   auto act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::connect);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::parallel_group);

   // This exits because the operation gets cancelled. Any receive operation gets cancelled
   act = fix.fsm.resume(fix.st, asio::error::operation_aborted, cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, run_action_type::cancel_receive);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, error_code(asio::error::operation_aborted));
}

// An error in connect with reconnection enabled triggers a reconnection
void test_connect_error()
{
   // Setup
   fixture fix;

   // Launch the operation
   auto act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::connect);

   // Connect errors. We sleep and try to connect again
   act = fix.fsm.resume(fix.st, error::connect_timeout, cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::wait_for_reconnection);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::connect);

   // This time we succeed and we launch the parallel group
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::parallel_group);
}

// An error in connect without reconnection enabled makes the operation finish
void test_connect_error_no_reconnect()
{
   // Setup
   fixture fix{config_no_reconnect()};

   // Launch the operation
   auto act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::connect);

   // Connect errors. The operation finishes
   act = fix.fsm.resume(fix.st, error::connect_timeout, cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code(error::connect_timeout));
}

// A cancellation in connect makes the operation finish even with reconnection enabled
void test_connect_cancel()
{
   // Setup
   fixture fix;

   // Launch the operation
   auto act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::connect);

   // Connect cancelled. The operation finishes
   act = fix.fsm.resume(fix.st, asio::error::operation_aborted, cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, error_code(asio::error::operation_aborted));
}

// Same, but only the cancellation is set
void test_connect_cancel_edge()
{
   // Setup
   fixture fix;

   // Launch the operation
   auto act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::connect);

   // Connect cancelled. The operation finishes
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, error_code(asio::error::operation_aborted));
}

}  // namespace

int main()
{
   test_success();
   test_success_no_reconnect();

   test_connect_error();
   test_connect_error_no_reconnect();
   test_connect_cancel();
   test_connect_cancel_edge();

   return boost::report_errors();
}
