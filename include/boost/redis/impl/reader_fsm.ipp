/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/detail/coroutine.hpp>
#include <boost/redis/detail/multiplexer.hpp>
#include <boost/redis/detail/read_buffer.hpp>
#include <boost/redis/detail/reader_fsm.hpp>

namespace boost::redis::detail {

reader_fsm::reader_fsm(read_buffer& rbuf, multiplexer& mpx) noexcept
: read_buffer_{&rbuf}
, mpx_{&mpx}
{ }

// TODO: write cancellation tests
reader_fsm::action reader_fsm::resume(
   std::size_t bytes_read,
   system::error_code ec,
   asio::cancellation_type_t cancel_state)
{
   switch (resume_point_) {
      BOOST_REDIS_CORO_INITIAL
      BOOST_REDIS_YIELD(resume_point_, 1, action::type::setup_cancellation)

      for (;;) {
         // Prepare the buffer for the read operation
         ec = read_buffer_->prepare_append();
         if (ec) {
            return {action::type::done, 0, ec};
         }

         // Read
         BOOST_REDIS_YIELD(resume_point_, 3, next_read_type_)

         // Process the bytes read, even if there was an error
         read_buffer_->commit_append(bytes_read);

         // Check for read errors
         if (ec) {
            // TODO: If an error occurred but data was read (i.e.
            // bytes_read != 0) we should try to process that data and
            // deliver it to the user before calling cancel_run.
            return {action::type::done, bytes_read, ec};
         }

         next_read_type_ = action::type::append_some;

         // Process the data that we've read
         while (read_buffer_->get_committed_size() != 0) {
            res_ = mpx_->consume_next(read_buffer_->get_committed_buffer(), ec);
            if (ec) {
               // TODO: Perhaps log what has not been consumed to aid
               // debugging.
               return {action::type::done, res_.second, ec};
            }

            if (res_.first == consume_result::needs_more) {
               next_read_type_ = action::type::needs_more;
               break;
            }

            read_buffer_->consume_committed(res_.second);

            if (res_.first == consume_result::got_push) {
               BOOST_REDIS_YIELD(resume_point_, 6, action::type::notify_push_receiver, res_.second)
               if (ec) {
                  return {action::type::done, 0u, ec};
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
   return {action::type::done, 0, system::error_code()};
}

}  // namespace boost::redis::detail
