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

#include <cstddef>
#include <string_view>

// Sans-io algorithm for the writer task, as a finite state machine

namespace boost::redis::detail {

// Forward decls
class connection_logger;
class multiplexer;

// What should we do next?
enum class writer_action_type
{
   done,   // Call the final handler
   write,  // Issue a write on the stream
   wait,   // Wait until there is data to be written
};

class writer_action {
   writer_action_type type_;
   union {
      system::error_code ec_;
      std::string_view write_buffer_;
   };

public:
   writer_action(writer_action_type type) noexcept
   : type_{type}
   { }

   writer_action_type type() const { return type_; }

   writer_action(system::error_code ec) noexcept
   : type_{writer_action_type::done}
   , ec_{ec}
   { }

   static writer_action write(std::string_view buffer)
   {
      auto res = writer_action{writer_action_type::write};
      res.write_buffer_ = buffer;
      return res;
   }

   system::error_code error() const
   {
      BOOST_ASSERT(type_ == writer_action_type::done);
      return ec_;
   }

   std::string_view write_buffer() const
   {
      BOOST_ASSERT(type_ == writer_action_type::write);
      return write_buffer_;
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
