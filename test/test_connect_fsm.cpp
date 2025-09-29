//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/detail/connect_fsm.hpp>

#include <boost/core/lightweight_test.hpp>

#include "boost/asio/ip/tcp.hpp"
#include "boost/redis/config.hpp"
#include "boost/redis/detail/connection_logger.hpp"
#include "boost/redis/logger.hpp"
#include "boost/system/detail/error_code.hpp"

#include <iterator>
#include <string>
#include <string_view>
#include <utility>
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

const tcp::endpoint endpoint(asio::ip::make_address("192.168.10.1"), 1234);

auto resolver_results = [] {
   const tcp::endpoint data[] = {endpoint};
   return asio::ip::tcp::resolver::results_type::create(
      std::begin(data),
      std::end(data),
      "my_host",
      "1234");
}();

void test_success_tcp()
{
   // Setup
   config cfg;
   std::ostringstream oss;
   std::vector<std::pair<logger::level, std::string>> msgs;
   detail::connection_logger lgr(
      logger(logger::level::debug, [&](logger::level lvl, std::string_view msg) {
         msgs.emplace_back(lvl, msg);
      }));
   connect_fsm fsm(cfg, lgr);
   redis_stream_state st{};

   // Initiate
   auto act = fsm.resume(error_code(), st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_resolve);
   act = fsm.resume(error_code(), resolver_results, st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_connect);
   act = fsm.resume(error_code(), endpoint, st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::done);

   // The transport type was appropriately set
   BOOST_TEST_EQ(st.type, transport_type::tcp);

   // TODO: check logging
}

}  // namespace

int main()
{
   test_success_tcp();

   return boost::report_errors();
}
