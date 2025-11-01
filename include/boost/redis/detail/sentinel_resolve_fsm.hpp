//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_SENTINEL_RESOLVE_FSM_HPP
#define BOOST_REDIS_SENTINEL_RESOLVE_FSM_HPP

#include <boost/redis/adapter/any_adapter.hpp>
#include <boost/redis/detail/connect_params.hpp>

#include <boost/asio/cancellation_type.hpp>
#include <boost/assert.hpp>
#include <boost/system/error_code.hpp>

// Sans-io algorithm for async_sentinel_resolve, as a finite state machine

namespace boost::redis::detail {

// Forward decls
struct connection_state;

// What should we do next?
enum class sentinel_action_type
{
   done,     // Call the final handler
   connect,  // Transport connection establishment
   request,  // Send the Sentinel request
};

class sentinel_action {
   sentinel_action_type type_;
   union {
      system::error_code ec_;
      connect_params connect_;
   };

   sentinel_action(sentinel_action_type type) noexcept
   : type_(type)
   { }

public:
   sentinel_action(system::error_code ec) noexcept
   : type_(sentinel_action_type::done)
   , ec_(ec)
   { }

   sentinel_action(const connect_params& params) noexcept
   : type_(sentinel_action_type::connect)
   , connect_(params)
   { }

   static sentinel_action request() { return {sentinel_action_type::request}; }

   sentinel_action_type type() const { return type_; }

   system::error_code error() const
   {
      BOOST_ASSERT(type_ == sentinel_action_type::done);
      return ec_;
   }

   const connect_params& get_connect_params() const
   {
      BOOST_ASSERT(type_ == sentinel_action_type::connect);
      return connect_;
   }
};

class sentinel_resolve_fsm {
   int resume_point_{0};
   std::size_t idx_{0u};
   bool unknown_master_{false};

public:
   sentinel_resolve_fsm() = default;

   sentinel_action resume(
      connection_state& st,
      system::error_code ec,
      asio::cancellation_type_t cancel_state);
};

any_adapter make_sentinel_adapter(connection_state& st);

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_CONNECTOR_HPP
