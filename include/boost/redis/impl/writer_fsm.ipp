//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_WRITER_FSM_IPP
#define BOOST_REDIS_WRITER_FSM_IPP

#include <boost/redis/detail/connection_logger.hpp>
#include <boost/redis/detail/coroutine.hpp>
#include <boost/redis/detail/multiplexer.hpp>
#include <boost/redis/detail/writer_fsm.hpp>
#include <boost/redis/impl/is_terminal_cancel.hpp>

#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/error.hpp>
#include <boost/assert.hpp>
#include <boost/system/error_code.hpp>

#include <cstddef>

namespace boost::redis::detail {

writer_action writer_fsm::resume(
   system::error_code ec,
   std::size_t bytes_written,
   asio::cancellation_type_t cancel_state)
{
   switch (resume_point_) {
      BOOST_REDIS_CORO_INITIAL

      for (;;) {
         // Attempt to write while we have requests ready to send
         while (mpx_->prepare_write() != 0u) {
            // Write
            BOOST_REDIS_YIELD(resume_point_, 1, writer_action_type::write)

            // Check for cancellations
            if (is_terminal_cancel(cancel_state)) {
               logger_->trace("Writer task cancelled (1).");
               return system::error_code(asio::error::operation_aborted);
            }

            // Check for errors
            if (ec) {
               logger_->on_write(ec, bytes_written);
               return ec;
            }

            // Mark requests as written
            mpx_->commit_write();
         }

         // No more requests ready to be written. Wait for more
         BOOST_REDIS_YIELD(resume_point_, 2, writer_action_type::wait)

         // Check for cancellations
         if (is_terminal_cancel(cancel_state)) {
            logger_->trace("Writer task cancelled (2).");
            return system::error_code(asio::error::operation_aborted);
         }
      }
   }

   // We should never reach here
   BOOST_ASSERT(false);
   return system::error_code();
}

}  // namespace boost::redis::detail

#endif
