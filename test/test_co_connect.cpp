//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/config.hpp>
#include <boost/redis/detail/connect_params.hpp>
#include <boost/redis/error.hpp>
#include <boost/redis/impl/co_connect.hpp>
#include <boost/redis/logger.hpp>

#include <boost/capy/io_task.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/corosio/endpoint.hpp>
#include <boost/corosio/ipv4_address.hpp>
#include <boost/corosio/resolver_results.hpp>

#include "corosio_common.hpp"
#include "sansio_utils.hpp"

#include <iostream>
#include <ostream>
#include <string_view>
#include <system_error>
#include <vector>

using namespace boost::redis;
using namespace boost::redis::test;
namespace capy = boost::capy;
namespace corosio = boost::corosio;
using detail::co_connect;
using detail::buffered_logger;
using detail::transport_type;
using detail::any_address_view;
using detail::connect_params;
using boost::system::error_code;
using namespace std::chrono_literals;

// Operators
static const char* to_string(transport_type type)
{
   switch (type) {
      case transport_type::tcp:         return "transport_type::tcp";
      case transport_type::tcp_tls:     return "transport_type::tcp_tls";
      case transport_type::unix_socket: return "transport_type::unix_socket";
      default:                          return "<unknown transport_type>";
   }
}

namespace boost::redis::detail {

std::ostream& operator<<(std::ostream& os, transport_type type) { return os << to_string(type); }

}  // namespace boost::redis::detail

namespace {

// Parses an endpoint and checks its return value
corosio::endpoint make_endpoint(std::string_view ep)
{
   corosio::endpoint res;
   auto ec = corosio::parse_endpoint(ep, res);
   if (!BOOST_TEST_EQ(ec, std::error_code()))
      std::cerr << "  Endpoint was: " << ep << std::endl;
   return res;
}

// Easier way to create a connect_params
connect_params make_connect_params(any_address_view addr)
{
   return {
      .addr = addr,
      .resolve_timeout = 10s,
      .connect_timeout = 10s,
      .ssl_handshake_timeout = 10s,
   };
}

// Mocking infrastructure
struct num_calls {
   int setup_unix{}, setup_tcp{}, setup_tcp_tls{}, unix_connect{}, tcp_resolve{}, tcp_connect{},
      tls_handshake{};

   friend bool operator==(const num_calls&, const num_calls&) = default;
};

std::ostream& operator<<(std::ostream& os, const num_calls& v)
{
   return os << "{ .setup_unix=" << v.setup_unix << ", .setup_tcp=" << v.setup_tcp
             << ", .setup_tcp_tls=" << v.setup_tcp_tls << ", .unix_connect=" << v.unix_connect
             << ", .tcp_resolve=" << v.tcp_resolve << ", .tcp_connect=" << v.tcp_connect
             << ", .tls_handshake=" << v.tls_handshake << " }";
}

struct return_value {
   std::error_code unix_connect{}, tcp_resolve{}, tcp_connect{}, tls_handshake{};
};

struct mock_impl {
   num_calls calls;
   return_value retval;

   void setup_unix() { ++calls.setup_unix; }
   void setup_tcp() { ++calls.setup_tcp; }
   void setup_tcp_tls() { ++calls.setup_tcp_tls; }

   capy::io_task<> unix_connect(const connect_params&)
   {
      ++calls.unix_connect;
      co_return retval.unix_connect;
   }

   capy::io_task<corosio::resolver_results> tcp_resolve(const connect_params&)
   {
      ++calls.tcp_resolve;
      std::vector<corosio::resolver_entry> entries{
         corosio::resolver_entry{make_endpoint("192.168.10.1:1234"), "my_host", "1234"},
         corosio::resolver_entry{make_endpoint("192.168.10.2:1235"), "my_host", "1234"},
      };
      co_return {retval.tcp_resolve, corosio::resolver_results{std::move(entries)}};
   }

   capy::io_task<corosio::endpoint> tcp_connect(
      const connect_params&,
      const corosio::resolver_results& results)
   {
      BOOST_TEST_NE(results.size(), 0u);
      ++calls.tcp_connect;
      co_return {retval.tcp_connect, *results.begin()};
   }

   capy::io_task<> tls_handshake(const connect_params&)
   {
      ++calls.tls_handshake;
      co_return retval.tls_handshake;
   }
};

// Reduce duplication
struct fixture : detail::log_fixture {
   buffered_logger lgr{make_logger()};
   mock_impl impl;
};

capy::task<> test_tcp_success()
{
   // Setup
   fixture fix;
   address addr{"some.host", "1234"};

   // Call the function
   auto [ec] = co_await co_connect(fix.impl, make_connect_params({addr, false}), fix.lgr);
   BOOST_TEST_EQ(ec, std::error_code());

   // Mock expectations
   constexpr num_calls expected_calls{
      .setup_tcp = 1,
      .tcp_resolve = 1,
      .tcp_connect = 1,
   };
   BOOST_TEST_EQ(fix.impl.calls, expected_calls);

   // Log
   fix.check_log({
      // clang-format off
      {logger::level::debug, "Connect: hostname resolution results: 192.168.10.1:1234, 192.168.10.2:1235"},
      {logger::level::debug, "Connect: TCP connect succeeded. Selected endpoint: 192.168.10.1:1234"      },
      // clang-format on
   });
}

capy::task<> test_tcp_tls_success()
{
   // Setup
   fixture fix;
   address addr{"some.host", "1234"};

   // Call the function
   auto [ec] = co_await co_connect(fix.impl, make_connect_params({addr, true}), fix.lgr);
   BOOST_TEST_EQ(ec, std::error_code());

   // Mock expectations
   constexpr num_calls expected_calls{
      .setup_tcp_tls = 1,
      .tcp_resolve = 1,
      .tcp_connect = 1,
      .tls_handshake = 1,
   };
   BOOST_TEST_EQ(fix.impl.calls, expected_calls);

   // Log
   fix.check_log({
      // clang-format off
      {logger::level::debug, "Connect: hostname resolution results: 192.168.10.1:1234, 192.168.10.2:1235"},
      {logger::level::debug, "Connect: TCP connect succeeded. Selected endpoint: 192.168.10.1:1234"      },
      {logger::level::debug, "Connect: SSL handshake succeeded"                                          },
      // clang-format on
   });
}

capy::task<> test_unix_success()
{
   // Setup
   fixture fix;

   // Call the function
   auto [ec] = co_await co_connect(
      fix.impl,
      make_connect_params(any_address_view{"/tmp/redis.sock"}),
      fix.lgr);
   BOOST_TEST_EQ(ec, std::error_code());

   // Mock expectations
   constexpr num_calls expected_calls{
      .setup_unix = 1,
      .unix_connect = 1,
   };
   BOOST_TEST_EQ(fix.impl.calls, expected_calls);

   // Log
   fix.check_log({
      {logger::level::debug, "Connect: UNIX socket connect succeeded"},
   });
}

// Resolve errors
capy::task<> test_tcp_resolve_error()
{
   // Setup
   fixture fix;
   fix.impl.retval.tcp_resolve = error::empty_field;
   address addr{"some.host", "1234"};

   // Call the function
   auto [ec] = co_await co_connect(fix.impl, make_connect_params({addr, false}), fix.lgr);
   BOOST_TEST_EQ(ec, error_code(error::empty_field));

   // Mock expectations
   constexpr num_calls expected_calls{
      .setup_tcp = 1,
      .tcp_resolve = 1,
   };
   BOOST_TEST_EQ(fix.impl.calls, expected_calls);

   // Log
   fix.check_log({
      // clang-format off
      {logger::level::info, "Connect: hostname resolution failed: Expected field value is empty. [boost.redis:5]"},
      // clang-format on
   });
}

// Connect errors
capy::task<> test_tcp_connect_error()
{
   // Setup
   fixture fix;
   fix.impl.retval.tcp_connect = error::empty_field;
   address addr{"some.host", "1234"};

   // Call the function
   auto [ec] = co_await co_connect(fix.impl, make_connect_params({addr, false}), fix.lgr);
   BOOST_TEST_EQ(ec, error_code(error::empty_field));

   // Mock expectations
   constexpr num_calls expected_calls{
      .setup_tcp = 1,
      .tcp_resolve = 1,
      .tcp_connect = 1,
   };
   BOOST_TEST_EQ(fix.impl.calls, expected_calls);

   // Log
   fix.check_log({
      // clang-format off
      {logger::level::debug, "Connect: hostname resolution results: 192.168.10.1:1234, 192.168.10.2:1235"},
      {logger::level::info,  "Connect: TCP connect failed: Expected field value is empty. [boost.redis:5]"},
      // clang-format on
   });
}

// SSL handshake error
capy::task<> test_ssl_handshake_error()
{
   // Setup
   fixture fix;
   fix.impl.retval.tls_handshake = error::empty_field;
   address addr{"some.host", "1234"};

   // Call the function
   auto [ec] = co_await co_connect(fix.impl, make_connect_params({addr, true}), fix.lgr);
   BOOST_TEST_EQ(ec, error_code(error::empty_field));

   // Mock expectations
   constexpr num_calls expected_calls{
      .setup_tcp_tls = 1,
      .tcp_resolve = 1,
      .tcp_connect = 1,
      .tls_handshake = 1,
   };
   BOOST_TEST_EQ(fix.impl.calls, expected_calls);

   // Log
   fix.check_log({
      // clang-format off
      {logger::level::debug, "Connect: hostname resolution results: 192.168.10.1:1234, 192.168.10.2:1235"},
      {logger::level::debug, "Connect: TCP connect succeeded. Selected endpoint: 192.168.10.1:1234"      },
      {logger::level::info,  "Connect: SSL handshake failed: Expected field value is empty. [boost.redis:5]"},
      // clang-format on
   });
}

// UNIX connect errors
capy::task<> test_unix_connect_error()
{
   // Setup
   fixture fix;
   fix.impl.retval.unix_connect = error::empty_field;

   // Call the function
   auto [ec] = co_await co_connect(
      fix.impl,
      make_connect_params(any_address_view{"/tmp/redis.sock"}),
      fix.lgr);
   BOOST_TEST_EQ(ec, error_code(error::empty_field));

   // Mock expectations
   constexpr num_calls expected_calls{
      .setup_unix = 1,
      .unix_connect = 1,
   };
   BOOST_TEST_EQ(fix.impl.calls, expected_calls);

   // Log
   fix.check_log({
      // clang-format off
      {logger::level::info, "Connect: UNIX socket connect failed: Expected field value is empty. [boost.redis:5]"},
      // clang-format on
   });
}

}  // namespace

int main()
{
   run_coroutine_test(test_tcp_success());
   run_coroutine_test(test_tcp_tls_success());
   run_coroutine_test(test_unix_success());

   run_coroutine_test(test_tcp_resolve_error());
   run_coroutine_test(test_tcp_connect_error());
   run_coroutine_test(test_ssl_handshake_error());
   run_coroutine_test(test_unix_connect_error());

   return boost::report_errors();
}
