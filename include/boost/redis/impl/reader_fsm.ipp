/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/detail/connection_state.hpp>
#include <boost/redis/detail/coroutine.hpp>
#include <boost/redis/detail/multiplexer.hpp>
#include <boost/redis/detail/reader_fsm.hpp>
#include <boost/redis/impl/is_terminal_cancel.hpp>

#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/error.hpp>

namespace boost::redis::detail {

reader_fsm::action reader_fsm::resume(
   connection_state& st,
   std::size_t bytes_read,
   system::error_code ec,
   asio::cancellation_type_t cancel_state)
{
   switch (resume_point_) {
      BOOST_REDIS_CORO_INITIAL

      for (;;) {
         // Prepare the buffer for the read operation
         ec = st.mpx.prepare_read();
         if (ec) {
            st.logger.trace("Reader task: error in prepare_read", ec);
            return {ec};
         }

         // Read
         st.logger.trace("Reader task: issuing a read operation");
         BOOST_REDIS_YIELD(resume_point_, 1, action::type::read_some)
         st.logger.on_read(ec, bytes_read);

         // Process the bytes read, even if there was an error
         st.mpx.commit_read(bytes_read);

         // Check for read errors
         if (ec) {
            // TODO: If an error occurred but data was read (i.e.
            // bytes_read != 0) we should try to process that data and
            // deliver it to the user before calling cancel_run.
            return {ec};
         }

         // Check for cancellations
         if (is_terminal_cancel(cancel_state)) {
            st.logger.trace("Reader task: cancelled (1)");
            return {asio::error::operation_aborted};
         }

         // Process the data that we've read
         while (st.mpx.get_read_buffer_size() != 0) {
            res_ = st.mpx.consume(ec);

            if (ec) {
               // TODO: Perhaps log what has not been consumed to aid
               // debugging.
               st.logger.trace("Reader task: error while processing message", ec);
               return {ec};
            }

            if (res_.first == consume_result::needs_more) {
               st.logger.trace("Reader task: incomplete message received");
               break;
            }

            if (res_.first == consume_result::got_push) {
               BOOST_REDIS_YIELD(resume_point_, 2, action::notify_push_receiver(res_.second))
               if (ec) {
                  return {ec};
               }
               if (is_terminal_cancel(cancel_state)) {
                  st.logger.trace("Reader task: cancelled (2)");
                  return {asio::error::operation_aborted};
               }
            } else {
               // TODO: Here we should notify the exec operation that
               // it can be completed. This will improve log clarity
               // and will make this code symmetrical in how it
               // handles pushes and other messages. The new action
               // type can be named notify_exec. To do that we need to
               // refactor the multiplexer.
            }
         }
      }
   }

   BOOST_ASSERT(false);
   return {system::error_code()};
}

}  // namespace boost::redis::detail
