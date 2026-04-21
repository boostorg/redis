//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_CO_CONNECT_FSM_HPP
#define BOOST_REDIS_CO_CONNECT_FSM_HPP

#include <boost/redis/detail/transport_type.hpp>

#include <boost/corosio/endpoint.hpp>
#include <boost/corosio/resolver_results.hpp>
#include <boost/system/error_code.hpp>

#include <span>

// Sans-io algorithm for redis_stream::async_connect, as a finite state machine

namespace boost::redis::detail {

struct buffered_logger;

struct co_redis_stream_state {
   transport_type type{transport_type::tcp};
   bool ssl_stream_used{false};
};

// What should we do next?
enum class co_connect_action_type
{
   unix_socket_close,    // Close the UNIX socket, to discard state
   unix_socket_connect,  // Connect to the UNIX socket
   tcp_resolve,          // Name resolution
   tcp_connect,          // TCP connect
   ssl_stream_reset,     // Re-create the SSL stream, to discard state
   ssl_handshake,        // SSL handshake
   done,                 // Complete the async op
};

struct co_connect_action {
   co_connect_action_type type;
   system::error_code ec;

   co_connect_action(co_connect_action_type type) noexcept
   : type{type}
   { }

   co_connect_action(system::error_code ec) noexcept
   : type{co_connect_action_type::done}
   , ec{ec}
   { }
};

class co_connect_fsm {
   int resume_point_{0};
   buffered_logger* lgr_{nullptr};

public:
   co_connect_fsm(buffered_logger& lgr) noexcept
   : lgr_(&lgr)
   { }

   co_connect_action resume(
      system::error_code ec,
      std::span<const corosio::resolver_entry> resolver_results,
      co_redis_stream_state& st);

   co_connect_action resume(
      system::error_code ec,
      const corosio::endpoint& selected_endpoint,
      co_redis_stream_state& st);

   co_connect_action resume(system::error_code ec, co_redis_stream_state& st);

};  // namespace boost::redis::detail

}  // namespace boost::redis::detail

#endif
