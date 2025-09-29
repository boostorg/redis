//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_CONNECT_FSM_HPP
#define BOOST_REDIS_CONNECT_FSM_HPP

#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>

// Sans-io algorithm for redis_stream::async_connect, as a finite state machine

namespace boost::redis {

struct config;

}

namespace boost::redis::detail {

class connection_logger;

// What transport is redis_stream using?
enum class transport_type
{
   tcp,          // plaintext TCP
   tcp_tls,      // TLS over TCP
   unix_socket,  // UNIX domain sockets
};

struct redis_stream_state {
   transport_type type{transport_type::tcp};
   bool ssl_stream_used{false};
};

// What should we do next?
enum class connect_action_type
{
   unix_socket_connect,
   tcp_resolve,
   tcp_connect,
   ssl_stream_reset,
   ssl_handshake,
   done,
};

class connect_action {
   connect_action_type type_;
   system::error_code ec_;

public:
   connect_action(connect_action_type type) noexcept
   : type_{type}
   { }

   connect_action(system::error_code ec) noexcept
   : type_{connect_action_type::done}
   , ec_{ec}
   { }

   connect_action_type type() const { return type_; }
   system::error_code error() const { return ec_; }
};

class connect_fsm {
   int resume_point_{0};
   const config* cfg_{nullptr};
   connection_logger* lgr_{nullptr};

public:
   connect_fsm(const config& cfg, connection_logger& lgr) noexcept
   : cfg_(&cfg)
   , lgr_(&lgr)
   { }

   const config& get_config() const { return *cfg_; }

   connect_action resume(
      system::error_code ec,
      const asio::ip::tcp::resolver::results_type& resolver_results,
      redis_stream_state& st,
      asio::cancellation_type_t cancel_state);

   connect_action resume(
      system::error_code ec,
      const asio::ip::tcp::endpoint& selected_endpoint,
      redis_stream_state& st,
      asio::cancellation_type_t cancel_state);

   connect_action resume(
      system::error_code ec,
      redis_stream_state& st,
      asio::cancellation_type_t cancel_state);

};  // namespace boost::redis::detail

}  // namespace boost::redis::detail

#endif
