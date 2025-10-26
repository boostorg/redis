//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_SENTINEL_RESOLVE_FSM_HPP
#define BOOST_REDIS_SENTINEL_RESOLVE_FSM_HPP

#include <boost/redis/config.hpp>

#include <boost/asio/cancellation_type.hpp>
#include <boost/system/error_code.hpp>

#include <string_view>

// Sans-io algorithm for async_sentinel_resolve, as a finite state machine

namespace boost::redis::detail {

// Forward decls
struct connection_state;

// What should we do next?
enum class sentinel_action_type
{
   done,     // Call the final handler
   connect,  // Transport connection establishment
   write,    // Transport write
   read,     // Transport read
};

class sentinel_action {
   sentinel_action_type type_;
   union {
      system::error_code ec_;
      const address* addr_;
      std::string_view write_buffer_;
   };

   sentinel_action(const address& addr) noexcept
   : type_(sentinel_action_type::connect)
   , addr_(&addr)
   { }

   sentinel_action(std::string_view write_buffer) noexcept
   : type_(sentinel_action_type::write)
   , write_buffer_(write_buffer)
   { }

   sentinel_action(sentinel_action_type type) noexcept
   : type_(type)
   { }

public:
   sentinel_action(system::error_code ec) noexcept
   : type_(sentinel_action_type::done)
   , ec_(ec)
   { }

   static sentinel_action write(std::string_view buffer) { return {buffer}; }
   static sentinel_action read() { return {sentinel_action_type::read}; }
   static sentinel_action connect(const address& addr) { return {addr}; }

   sentinel_action_type type() const { return type_; }
};

class sentinel_resolve_fsm {
   int resume_point_{0};
   std::size_t idx_{0u};

public:
   sentinel_resolve_fsm() = default;

   sentinel_action resume(
      connection_state& st,
      system::error_code ec,
      asio::cancellation_type_t cancel_state);
};

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_CONNECTOR_HPP
