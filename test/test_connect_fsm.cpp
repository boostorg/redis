//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/detail/connect_fsm.hpp>

#include <boost/core/lightweight_test.hpp>

#include "boost/asio/error.hpp"
#include "boost/asio/ip/tcp.hpp"
#include "boost/redis/config.hpp"
#include "boost/redis/detail/connection_logger.hpp"
#include "boost/redis/logger.hpp"
#include "boost/system/detail/error_code.hpp"

#include <iterator>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

using namespace boost::redis;
namespace asio = boost::asio;
using detail::connect_fsm;
using detail::connect_action_type;
using detail::connect_action;
using detail::connection_logger;
using detail::redis_stream_state;
using detail::transport_type;
using asio::ip::tcp;
using boost::system::error_code;
using boost::asio::cancellation_type_t;
using resolver_results = tcp::resolver::results_type;

// Operators
static const char* to_string(connect_action_type type)
{
   switch (type) {
      case connect_action_type::unix_socket_connect:
         return "connect_action_type::unix_socket_connect";
      case connect_action_type::tcp_resolve:      return "connect_action_type::tcp_resolve";
      case connect_action_type::tcp_connect:      return "connect_action_type::tcp_connect";
      case connect_action_type::ssl_stream_reset: return "connect_action_type::ssl_stream_reset";
      case connect_action_type::ssl_handshake:    return "connect_action_type::ssl_handshake";
      case connect_action_type::done:             return "connect_action_type::done";
      default:                                    return "<unknown connect_action_type>";
   }
}

static const char* to_string(transport_type type)
{
   switch (type) {
      case transport_type::tcp:         return "transport_type::tcp";
      case transport_type::tcp_tls:     return "transport_type::tcp_tls";
      case transport_type::unix_socket: return "transport_type::unix_socket";
      default:                          return "<unknown transport_type>";
   }
}

static const char* to_string(logger::level lvl)
{
   switch (lvl) {
      case logger::level::disabled: return "logger::level::disabled";
      case logger::level::emerg:    return "logger::level::emerg";
      case logger::level::alert:    return "logger::level::alert";
      case logger::level::crit:     return "logger::level::crit";
      case logger::level::err:      return "logger::level::err";
      case logger::level::warning:  return "logger::level::warning";
      case logger::level::notice:   return "logger::level::notice";
      case logger::level::info:     return "logger::level::info";
      case logger::level::debug:    return "logger::level::debug";
      default:                      return "<unknown logger::level>";
   }
}

namespace boost::redis {

std::ostream& operator<<(std::ostream& os, logger::level lvl) { return os << to_string(lvl); }

}  // namespace boost::redis

namespace boost::redis::detail {

std::ostream& operator<<(std::ostream& os, connect_action_type type)
{
   return os << to_string(type);
}

std::ostream& operator<<(std::ostream& os, transport_type type) { return os << to_string(type); }

bool operator==(const connect_action& lhs, const connect_action& rhs) noexcept
{
   return lhs.type() == rhs.type() && lhs.error() == rhs.error();
}

std::ostream& operator<<(std::ostream& os, const connect_action& act)
{
   os << "connect_action{ .type=" << act.type();
   if (act.type() == connect_action_type::done)
      os << ", .error=" << act.error();
   return os << " }";
}

}  // namespace boost::redis::detail

namespace {

// TCP endpoints
const tcp::endpoint endpoint(asio::ip::make_address("192.168.10.1"), 1234);
const tcp::endpoint endpoint2(asio::ip::make_address("192.168.10.2"), 1235);

auto resolver_data = [] {
   const tcp::endpoint data[] = {endpoint, endpoint2};
   return asio::ip::tcp::resolver::results_type::create(
      std::begin(data),
      std::end(data),
      "my_host",
      "1234");
}();

// For checking logs
struct log_message {
   logger::level lvl;
   std::string msg;

   friend bool operator==(const log_message& lhs, const log_message& rhs) noexcept
   {
      return lhs.lvl == rhs.lvl && lhs.msg == rhs.msg;
   }

   friend std::ostream& operator<<(std::ostream& os, const log_message& v)
   {
      return os << "log_message { .lvl=" << v.lvl << ", .msg=" << v.msg << " }";
   }
};

// Reduce duplication
struct fixture {
   config cfg;
   std::ostringstream oss{};
   std::vector<log_message> msgs{};
   detail::connection_logger lgr{
      logger(logger::level::debug, [&](logger::level lvl, std::string_view msg) {
         msgs.push_back({lvl, std::string(msg)});
      })};
   connect_fsm fsm{cfg, lgr};
   redis_stream_state st{};
};

void test_tcp_success()
{
   // Setup
   fixture fix;

   // Run the algorithm
   auto act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_resolve);
   act = fix.fsm.resume(error_code(), resolver_data, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_connect);
   act = fix.fsm.resume(error_code(), endpoint, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::done);

   // The transport type was appropriately set
   BOOST_TEST_EQ(fix.st.type, transport_type::tcp);
   BOOST_TEST_NOT(fix.st.ssl_stream_used);

   // Check logging
   const log_message expected[] = {
      {logger::level::info, "Resolve results: 192.168.10.1:1234, 192.168.10.2:1235"},
      {logger::level::info, "Connected to 192.168.10.1:1234"                       },
   };
   BOOST_TEST_ALL_EQ(std::begin(expected), std::end(expected), fix.msgs.begin(), fix.msgs.end());
}

void test_tcp_tls_success()
{
   // Setup
   config cfg;
   cfg.use_ssl = true;
   fixture fix{std::move(cfg)};

   // Run the algorithm. No SSL stream reset is performed here
   auto act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_resolve);
   act = fix.fsm.resume(error_code(), resolver_data, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_connect);
   act = fix.fsm.resume(error_code(), endpoint, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::ssl_handshake);
   act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::done);

   // The transport type was appropriately set
   BOOST_TEST_EQ(fix.st.type, transport_type::tcp_tls);
   BOOST_TEST(fix.st.ssl_stream_used);

   // Check logging
   const log_message expected[] = {
      {logger::level::info, "Resolve results: 192.168.10.1:1234, 192.168.10.2:1235"},
      {logger::level::info, "Connected to 192.168.10.1:1234"                       },
      {logger::level::info, "Successfully performed SSL handshake"                 },
   };
   BOOST_TEST_ALL_EQ(std::begin(expected), std::end(expected), fix.msgs.begin(), fix.msgs.end());
}

void test_tcp_tls_success_reconnect()
{
   // Setup
   config cfg;
   cfg.use_ssl = true;
   fixture fix{std::move(cfg)};
   fix.st.ssl_stream_used = true;

   // Run the algorithm. The stream is used, so it needs to be reset
   auto act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::ssl_stream_reset);
   act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_resolve);
   act = fix.fsm.resume(error_code(), resolver_data, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_connect);
   act = fix.fsm.resume(error_code(), endpoint, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::ssl_handshake);
   act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::done);

   // The transport type was appropriately set
   BOOST_TEST_EQ(fix.st.type, transport_type::tcp_tls);
   BOOST_TEST(fix.st.ssl_stream_used);

   // Check logging
   const log_message expected[] = {
      {logger::level::info, "Resolve results: 192.168.10.1:1234, 192.168.10.2:1235"},
      {logger::level::info, "Connected to 192.168.10.1:1234"                       },
      {logger::level::info, "Successfully performed SSL handshake"                 },
   };
   BOOST_TEST_ALL_EQ(std::begin(expected), std::end(expected), fix.msgs.begin(), fix.msgs.end());
}

// Resolve errors
void test_tcp_resolve_error()
{
   // Setup
   fixture fix;

   // Run the algorithm
   auto act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_resolve);
   act = fix.fsm.resume(error::empty_field, resolver_results{}, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code(error::empty_field));

   // Check logging
   const log_message expected[] = {
      // clang-format off
      {logger::level::info, "Error resolving the server hostname: Expected field value is empty. [boost.redis:5]"},
      // clang-format on
   };
   BOOST_TEST_ALL_EQ(std::begin(expected), std::end(expected), fix.msgs.begin(), fix.msgs.end());
}

void test_tcp_resolve_timeout()
{
   // Setup
   fixture fix;

   // Since we use cancel_after, a timeout is an operation_aborted without a cancellation state set
   auto act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_resolve);
   act = fix.fsm.resume(
      asio::error::operation_aborted,
      resolver_results{},
      fix.st,
      cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code(error::resolve_timeout));

   // Check logging
   const log_message expected[] = {
      // clang-format off
      {logger::level::info, "Error resolving the server hostname: Resolve timeout. [boost.redis:17]"},
      // clang-format on
   };
   BOOST_TEST_ALL_EQ(std::begin(expected), std::end(expected), fix.msgs.begin(), fix.msgs.end());
}

void test_tcp_resolve_cancel()
{
   // Setup
   fixture fix;

   // Run the algorithm
   auto act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_resolve);
   act = fix.fsm.resume(
      asio::error::operation_aborted,
      resolver_results{},
      fix.st,
      cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, error_code(asio::error::operation_aborted));

   // Logging here is system-dependent, so we don't check the message
   BOOST_TEST_EQ(fix.msgs.size(), 1u);
}

void test_tcp_resolve_cancel_edge()
{
   // Setup
   fixture fix;

   // Cancel state set but no error
   auto act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_resolve);
   act = fix.fsm.resume(error_code(), resolver_results{}, fix.st, cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, error_code(asio::error::operation_aborted));

   // Logging here is system-dependent, so we don't check the message
   BOOST_TEST_EQ(fix.msgs.size(), 1u);
}

// Connect errors
void test_tcp_connect_error()
{
   // Setup
   fixture fix;

   // Run the algorithm
   auto act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_resolve);
   act = fix.fsm.resume(error_code(), resolver_data, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_connect);
   act = fix.fsm.resume(error::empty_field, tcp::endpoint{}, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code(error::empty_field));

   // Check logging
   const log_message expected[] = {
      // clang-format off
      {logger::level::info, "Resolve results: 192.168.10.1:1234, 192.168.10.2:1235"},
      {logger::level::info, "Failed to connect to the server: Expected field value is empty. [boost.redis:5]"},
      // clang-format on
   };
   BOOST_TEST_ALL_EQ(std::begin(expected), std::end(expected), fix.msgs.begin(), fix.msgs.end());
}

void test_tcp_connect_timeout()
{
   // Setup
   fixture fix;

   // Run the algorithm
   auto act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_resolve);
   act = fix.fsm.resume(error_code(), resolver_data, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_connect);
   act = fix.fsm.resume(
      asio::error::operation_aborted,
      tcp::endpoint{},
      fix.st,
      cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code(error::connect_timeout));

   // Check logging
   const log_message expected[] = {
      // clang-format off
      {logger::level::info, "Resolve results: 192.168.10.1:1234, 192.168.10.2:1235"},
      {logger::level::info, "Failed to connect to the server: Connect timeout. [boost.redis:18]"},
      // clang-format on
   };
   BOOST_TEST_ALL_EQ(std::begin(expected), std::end(expected), fix.msgs.begin(), fix.msgs.end());
}

void test_tcp_connect_cancel()
{
   // Setup
   fixture fix;

   // Run the algorithm
   auto act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_resolve);
   act = fix.fsm.resume(error_code(), resolver_data, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_connect);
   act = fix.fsm.resume(
      asio::error::operation_aborted,
      tcp::endpoint{},
      fix.st,
      cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, error_code(asio::error::operation_aborted));

   // Logging here is system-dependent, so we don't check the message
   BOOST_TEST_EQ(fix.msgs.size(), 2u);
}

void test_tcp_connect_cancel_edge()
{
   // Setup
   fixture fix;

   // Run the algorithm. Cancellation state set but no error
   auto act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_resolve);
   act = fix.fsm.resume(error_code(), resolver_data, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_connect);
   act = fix.fsm.resume(error_code(), tcp::endpoint{}, fix.st, cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, error_code(asio::error::operation_aborted));

   // Logging here is system-dependent, so we don't check the message
   BOOST_TEST_EQ(fix.msgs.size(), 2u);
}

// SSL handshake error
void test_ssl_handshake_error()
{
   // Setup
   config cfg;
   cfg.use_ssl = true;
   fixture fix{std::move(cfg)};

   // Run the algorithm. No SSL stream reset is performed here
   auto act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_resolve);
   act = fix.fsm.resume(error_code(), resolver_data, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_connect);
   act = fix.fsm.resume(error_code(), endpoint, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::ssl_handshake);
   act = fix.fsm.resume(error::empty_field, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code(error::empty_field));

   // The stream is marked as used
   BOOST_TEST(fix.st.ssl_stream_used);

   // Check logging
   const log_message expected[] = {
      // clang-format off
      {logger::level::info, "Resolve results: 192.168.10.1:1234, 192.168.10.2:1235"},
      {logger::level::info, "Connected to 192.168.10.1:1234"                       },
      {logger::level::info, "Failed to perform SSL handshake: Expected field value is empty. [boost.redis:5]"},
      // clang-format on
   };
   BOOST_TEST_ALL_EQ(std::begin(expected), std::end(expected), fix.msgs.begin(), fix.msgs.end());
}

void test_ssl_handshake_timeout()
{
   // Setup
   config cfg;
   cfg.use_ssl = true;
   fixture fix{std::move(cfg)};

   // Run the algorithm. Timeout = operation_aborted without the cancel type set
   auto act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_resolve);
   act = fix.fsm.resume(error_code(), resolver_data, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_connect);
   act = fix.fsm.resume(error_code(), endpoint, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::ssl_handshake);
   act = fix.fsm.resume(asio::error::operation_aborted, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code(error::ssl_handshake_timeout));

   // The stream is marked as used
   BOOST_TEST(fix.st.ssl_stream_used);

   // Check logging
   const log_message expected[] = {
      // clang-format off
      {logger::level::info, "Resolve results: 192.168.10.1:1234, 192.168.10.2:1235"},
      {logger::level::info, "Connected to 192.168.10.1:1234"                       },
      {logger::level::info, "Failed to perform SSL handshake: SSL handshake timeout. [boost.redis:20]"},
      // clang-format on
   };
   BOOST_TEST_ALL_EQ(std::begin(expected), std::end(expected), fix.msgs.begin(), fix.msgs.end());
}

void test_ssl_handshake_cancel()
{
   // Setup
   config cfg;
   cfg.use_ssl = true;
   fixture fix{std::move(cfg)};

   // Run the algorithm. Cancel = operation_aborted with the cancel type set
   auto act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_resolve);
   act = fix.fsm.resume(error_code(), resolver_data, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_connect);
   act = fix.fsm.resume(error_code(), endpoint, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::ssl_handshake);
   act = fix.fsm.resume(asio::error::operation_aborted, fix.st, cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, error_code(asio::error::operation_aborted));

   // The stream is marked as used
   BOOST_TEST(fix.st.ssl_stream_used);

   // Logging is system-dependent, so we don't check messages
   BOOST_TEST_EQ(fix.msgs.size(), 3u);
}

void test_ssl_handshake_cancel_edge()
{
   // Setup
   config cfg;
   cfg.use_ssl = true;
   fixture fix{std::move(cfg)};

   // Run the algorithm. No error, but the cancel state is set
   auto act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_resolve);
   act = fix.fsm.resume(error_code(), resolver_data, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_connect);
   act = fix.fsm.resume(error_code(), endpoint, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::ssl_handshake);
   act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, error_code(asio::error::operation_aborted));

   // The stream is marked as used
   BOOST_TEST(fix.st.ssl_stream_used);

   // Logging is system-dependent, so we don't check messages
   BOOST_TEST_EQ(fix.msgs.size(), 3u);
}

}  // namespace

int main()
{
   test_tcp_success();
   test_tcp_tls_success();
   test_tcp_tls_success_reconnect();

   test_tcp_resolve_error();
   test_tcp_resolve_timeout();
   test_tcp_resolve_cancel();
   test_tcp_resolve_cancel_edge();

   test_tcp_connect_error();
   test_tcp_connect_timeout();
   test_tcp_connect_cancel();
   test_tcp_connect_cancel_edge();

   test_ssl_handshake_error();
   test_ssl_handshake_timeout();
   test_ssl_handshake_cancel();
   test_ssl_handshake_cancel_edge();

   return boost::report_errors();
}
