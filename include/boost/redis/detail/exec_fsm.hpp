//
// Copyright (c) 2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_EXEC_FSM_HPP
#define BOOST_REDIS_EXEC_FSM_HPP

#include <boost/redis/detail/coroutine.hpp>
#include <boost/redis/detail/multiplexer.hpp>
#include <boost/redis/request.hpp>

#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/error.hpp>
#include <boost/assert.hpp>
#include <boost/system/error_code.hpp>

#include <cstddef>
#include <memory>

// Sans-io algorithm for async_exec, as a finite state machine

namespace boost::redis::detail {

// What should we do next?
enum class exec_action_type
{
   immediate,          // Invoke asio::async_immediate to avoid re-entrancy problems
   done,               // Call the final handler
   write,              // Notify the writer task
   wait_for_response,  // Wait to be notified
};

class exec_action {
   exec_action_type type_;
   system::error_code ec_;
   std::size_t bytes_read_;

public:
   exec_action(exec_action_type type) noexcept
   : type_{type}
   { }

   exec_action(system::error_code ec, std::size_t bytes_read = 0u) noexcept
   : type_{exec_action_type::done}
   , ec_{ec}
   , bytes_read_{bytes_read}
   { }

   exec_action_type type() const { return type_; }
   system::error_code error() const { return ec_; }
   std::size_t bytes_read() const { return bytes_read_; }
};

class exec_fsm {
   int resume_point_{0};
   multiplexer* mpx_{nullptr};
   std::shared_ptr<multiplexer::elem> elem_;

   exec_action resume_impl(bool connection_is_open, asio::cancellation_type_t cancel_state)
   {
      switch (resume_point_) {
         BOOST_REDIS_CORO_INITIAL

         // Check whether the user wants to wait for the connection to
         // be established.
         if (elem_->get_request().get_config().cancel_if_not_connected && !connection_is_open) {
            BOOST_REDIS_YIELD(resume_point_, 1, exec_action_type::immediate)
            return system::error_code(error::not_connected);
         }

         // Add the request to the multiplexer
         mpx_->add(elem_);

         // If the multiplexer is idle and the connection is open, trigger a write
         if (connection_is_open && !mpx_->is_writing())
            BOOST_REDIS_YIELD(resume_point_, 2, exec_action_type::write)

         // TODO: properly handle cancellation at this point

         while (true) {
            // Wait until we get notified. This will return once the request completes,
            // or upon any kind of cancellation
            BOOST_REDIS_YIELD(resume_point_, 3, exec_action_type::wait_for_response)

            // If the request has completed (with error or not), we're done
            if (elem_->is_done()) {
               return exec_action{
                  elem_->get_error(),
                  elem_->get_error() ? 0u : elem_->get_read_size()};
            }

            // We can only honor terminal cancellations here. If this is the case, clear the callback
            // so that when we get a response for this request, it does nothing
            // TODO: we could also honor other cancellation types here if the request is waiting
            if (!!(cancel_state & asio::cancellation_type_t::terminal)) {
               elem_->set_done_callback([] { });
               return exec_action{asio::error::operation_aborted};
            }

            // If we couldn't honor the requested cancellation type, go back to waiting
         }
      }

      // We should never get here
      BOOST_ASSERT(false);
      return exec_action{system::error_code()};
   }

public:
   exec_fsm(multiplexer& mpx, std::shared_ptr<multiplexer::elem> elem) noexcept
   : mpx_(&mpx)
   , elem_(std::move(elem))
   { }

   exec_action resume(bool connection_is_open, asio::cancellation_type_t cancel_state)
   {
      // When completing, we should deallocate any temporary storage we acquired
      // for the operation before invoking the final handler.
      // This intercepts the returned action to implement this.
      auto act = resume_impl(connection_is_open, cancel_state);
      if (act.type() == exec_action_type::done)
         elem_.reset();
      return act;
   }
};

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_CONNECTOR_HPP
