//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_WRITER_FSM_HPP
#define BOOST_REDIS_WRITER_FSM_HPP

#include <boost/redis/detail/connection_state.hpp>

#include <boost/asio/cancellation_type.hpp>
#include <boost/system/error_code.hpp>

#include <chrono>
#include <cstddef>

// Sans-io algorithm for the writer task, as a finite state machine

namespace boost::redis::detail {

// Forward decls
class connection_logger;
class multiplexer;

// What should we do next?
enum class writer_action_type
{
   done,        // Call the final handler
   write_some,  // Issue a write on the stream
   wait,        // Wait until there is data to be written
};

class writer_action {
   writer_action_type type_;
   union {
      system::error_code ec_;
      struct {
         std::size_t offset;
         std::chrono::steady_clock::duration timeout;
      } offset_timeout_;
   };

   writer_action(
      writer_action_type type,
      std::size_t offset,
      std::chrono::steady_clock::duration t) noexcept
   : type_{type}
   , offset_timeout_{offset, t}
   { }

public:
   writer_action_type type() const { return type_; }

   writer_action(system::error_code ec) noexcept
   : type_{writer_action_type::done}
   , ec_{ec}
   { }

   static writer_action write_some(std::size_t offset, std::chrono::steady_clock::duration timeout)
   {
      return {writer_action_type::write_some, offset, timeout};
   }

   static writer_action wait(std::chrono::steady_clock::duration timeout)
   {
      return {writer_action_type::wait, 0u, timeout};
   }

   system::error_code error() const
   {
      BOOST_ASSERT(type_ == writer_action_type::done);
      return ec_;
   }

   std::size_t write_offset() const
   {
      BOOST_ASSERT(type_ == writer_action_type::write_some);
      return offset_timeout_.offset;
   }

   std::chrono::steady_clock::duration timeout() const
   {
      BOOST_ASSERT(type_ == writer_action_type::write_some || type_ == writer_action_type::wait);
      return offset_timeout_.timeout;
   }
};

class writer_fsm {
   int resume_point_{0};
   std::size_t write_offset_{};

public:
   writer_fsm() noexcept = default;

   writer_action resume(
      connection_state& st,
      system::error_code ec,
      std::size_t bytes_written,
      asio::cancellation_type_t cancel_state);
};

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_CONNECTOR_HPP
