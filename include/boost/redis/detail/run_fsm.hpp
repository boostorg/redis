//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_RUN_FSM_HPP
#define BOOST_REDIS_RUN_FSM_HPP

#include <boost/redis/detail/connection_state.hpp>

#include <boost/asio/cancellation_type.hpp>
#include <boost/assert.hpp>
#include <boost/system/error_code.hpp>

#include <chrono>

// Sans-io algorithm for async_run, as a finite state machine

namespace boost::redis::detail {

// Forward decls
class connection_logger;
class multiplexer;

// What should we do next?
enum class run_action_type
{
   done,            // Call the final handler
   immediate,       // Call asio::async_immediate
   connect,         // Transport connection establishment
   parallel_group,  // Run the reader, writer and friends
   cancel_receive,  // Cancel the receiver channel
   sleep,           // Wait for some time
};

class run_action {
   run_action_type type_;
   union {
      system::error_code ec_;
      std::chrono::steady_clock::duration sleep_period_;
   };

public:
   run_action(run_action_type type) noexcept
   : type_{type}
   { }

   run_action(system::error_code ec) noexcept
   : type_{run_action_type::done}
   , ec_{ec}
   { }

   static run_action wait(std::chrono::steady_clock::duration period)
   {
      auto res = run_action(run_action_type::sleep);
      res.sleep_period_ = period;
      return res;
   }

   run_action_type type() const { return type_; }

   system::error_code error() const
   {
      BOOST_ASSERT(type_ == run_action_type::done);
      return ec_;
   }

   std::chrono::steady_clock::duration sleep_period() const
   {
      BOOST_ASSERT(type_ == run_action_type::sleep);
      return sleep_period_;
   }
};

class run_fsm {
   int resume_point_{0};

public:
   run_fsm() = default;

   run_action resume(
      connection_state& st,
      system::error_code ec,
      asio::cancellation_type_t cancel_state);
};

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_CONNECTOR_HPP
