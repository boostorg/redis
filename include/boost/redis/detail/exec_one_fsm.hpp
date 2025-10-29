//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_EXEC_FSM_HPP
#define BOOST_REDIS_EXEC_FSM_HPP

#include <boost/redis/adapter/any_adapter.hpp>
#include <boost/redis/detail/coroutine.hpp>
#include <boost/redis/detail/read_buffer.hpp>
#include <boost/redis/impl/is_terminal_cancel.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/resp3/node.hpp>
#include <boost/redis/resp3/parser.hpp>

#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/error.hpp>
#include <boost/assert.hpp>
#include <boost/system/error_code.hpp>

#include <cstddef>
#include <optional>

// Sans-io algorithm for async_exec_one, as a finite state machine

namespace boost::redis::detail {

// What should we do next?
enum class exec_one_action_type
{
   done,       // Call the final handler
   write,      // Write the request
   read_some,  // Read into the read buffer
};

struct exec_one_action {
   exec_one_action_type type;
   system::error_code ec;

   exec_one_action(exec_one_action_type type) noexcept
   : type{type}
   { }

   exec_one_action(system::error_code ec) noexcept
   : type{exec_one_action_type::done}
   , ec{ec}
   { }
};

class exec_one_fsm {
   int resume_point_{0};
   any_adapter resp_;
   std::size_t remaining_responses_;
   resp3::parser parser_;

public:
   exec_one_fsm(any_adapter resp, std::size_t expected_responses)
   : resp_(std::move(resp))
   , remaining_responses_(expected_responses)
   { }

   // TODO: move to ipp
   exec_one_action resume(
      read_buffer& buffer,
      system::error_code ec,
      std::size_t bytes_transferred,
      asio::cancellation_type_t cancel_state)
   {
      std::optional<resp3::node_view> node;

      switch (resume_point_) {
         BOOST_REDIS_CORO_INITIAL

         // Send the request to the server
         BOOST_REDIS_YIELD(resume_point_, 1, exec_one_action_type::write)

         // Errors and cancellations
         // TODO: timeouts?
         if (is_terminal_cancel(cancel_state))
            ec = asio::error::operation_aborted;
         if (ec)
            return ec;

         // Read responses until we're done
         for (; remaining_responses_ != 0u; --remaining_responses_) {
            // We've started this response
            resp_.on_init();

            while (!parser_.done()) {
               // Try to consume any cached data
               node = parser_.consume(buffer.get_commited(), ec);
               if (ec)
                  return ec;

               if (node) {
                  // We've got a valid node
                  resp_.on_node(*node, ec);
                  if (ec)
                     return ec;
               } else {
                  // We need to read more data. Prepare the buffer
                  ec = buffer.prepare();
                  if (ec)
                     return ec;

                  // Read data
                  BOOST_REDIS_YIELD(resume_point_, 2, exec_one_action_type::read_some)

                  // Errors and cancellations
                  // TODO: timeouts?
                  if (is_terminal_cancel(cancel_state))
                     ec = asio::error::operation_aborted;
                  if (ec)
                     return ec;

                  // Commit the data into the buffer
                  buffer.commit(bytes_transferred);
               }
            }

            // We've finished parsing this response
            resp_.on_done();
            buffer.consume(parser_.get_consumed());
            parser_.reset();
         }
      }

      BOOST_ASSERT(false);
      return system::error_code();
   }
};

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_CONNECTOR_HPP
