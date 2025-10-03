//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_WRITER_FSM_HPP
#define BOOST_REDIS_WRITER_FSM_HPP

#include <boost/asio/cancellation_type.hpp>
#include <boost/system/error_code.hpp>

#include <cstddef>

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

struct writer_action {
   writer_action_type type;
   system::error_code ec;

   writer_action(writer_action_type type) noexcept
   : type{type}
   { }

   writer_action(system::error_code ec) noexcept
   : type{writer_action_type::done}
   , ec{ec}
   { }
};

class writer_fsm {
   int resume_point_{0};
   multiplexer* mpx_;
   connection_logger* logger_;

public:
   writer_fsm(multiplexer& mpx, connection_logger& logger) noexcept
   : mpx_(&mpx)
   , logger_(&logger)
   { }

   writer_action resume(
      system::error_code ec,
      std::size_t bytes_written,
      asio::cancellation_type_t cancel_state);
};

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_CONNECTOR_HPP
