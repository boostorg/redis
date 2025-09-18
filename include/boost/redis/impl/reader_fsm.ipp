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

reader_fsm::action reader_fsm::resume(
   std::size_t bytes_read,
   system::error_code ec,
   asio::cancellation_type_t /*cancel_state*/)
{
   switch (resume_point_) {
      BOOST_REDIS_CORO_INITIAL
      BOOST_REDIS_YIELD(resume_point_, 1, action::type::setup_cancellation)

      for (;;) {
         ec = read_buffer_->prepare_append();
         if (ec) {
            action_after_resume_ = {action::type::done, 0, ec};
            BOOST_REDIS_YIELD(resume_point_, 2, action::type::cancel_run)
            return action_after_resume_;
         }

         BOOST_REDIS_YIELD(resume_point_, 3, next_read_type_)
         read_buffer_->commit_append(bytes_read);
         if (ec) {
            // TODO: If an error occurred but data was read (i.e.
            // bytes_read != 0) we should try to process that data and
            // deliver it to the user before calling cancel_run.
            action_after_resume_ = {action::type::done, bytes_read, ec};
            BOOST_REDIS_YIELD(resume_point_, 4, action::type::cancel_run)
            return action_after_resume_;
         }

         next_read_type_ = action::type::append_some;
         while (read_buffer_->get_committed_size() != 0) {
            res_ = mpx_->consume_next(read_buffer_->get_committed_buffer(), ec);
            if (ec) {
               // TODO: Perhaps log what has not been consumed to aid
               // debugging.
               action_after_resume_ = {action::type::done, res_.second, ec};
               BOOST_REDIS_YIELD(resume_point_, 5, action::type::cancel_run)
               return action_after_resume_;
            }

            if (res_.first == consume_result::needs_more) {
               next_read_type_ = action::type::needs_more;
               break;
            }

            read_buffer_->consume_committed(res_.second);

            if (res_.first == consume_result::got_push) {
               BOOST_REDIS_YIELD(resume_point_, 6, action::type::notify_push_receiver, res_.second)
               if (ec) {
                  action_after_resume_ = {action::type::done, 0u, ec};
                  BOOST_REDIS_YIELD(resume_point_, 7, action::type::cancel_run)
                  return action_after_resume_;
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
