/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/detail/coroutine.hpp>
#include <boost/redis/detail/multiplexer.hpp>
#include <boost/redis/detail/reader_fsm.hpp>

namespace boost::redis::detail {

reader_fsm::reader_fsm(multiplexer& mpx, push_notifier_type push_notifier) noexcept
: mpx_{&mpx}
, push_notifier_{std::move(push_notifier)}
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
         BOOST_REDIS_YIELD(resume_point_, 2, next_read_type_)
         if (ec) {
            // TODO: If an error occurred but data was read (i.e.
            // bytes_read != 0) we should try to process that data and
            // deliver it to the user before calling cancel_run.
            action_after_resume_ = {action::type::done, bytes_read, ec};
            BOOST_REDIS_YIELD(resume_point_, 3, action::type::cancel_run)
            return action_after_resume_;
         }

         next_read_type_ = action::type::append_some;
         while (!mpx_->get_read_buffer().empty()) {
            res_ = mpx_->consume_next(ec);
            if (ec) {
               action_after_resume_ = {action::type::done, res_.second, ec};
               BOOST_REDIS_YIELD(resume_point_, 4, action::type::cancel_run)
               return action_after_resume_;
            }

            if (!res_.first.has_value()) {
               next_read_type_ = action::type::needs_more;
               break;
            }

            if (!res_.first.value() && !push_notifier_(res_.second)) {
               BOOST_REDIS_YIELD(resume_point_, 6, action::type::notify_push_receiver, res_.second)
               if (ec) {
                  action_after_resume_ = {action::type::done, 0u, ec};
                  BOOST_REDIS_YIELD(resume_point_, 7, action::type::cancel_run)
                  return action_after_resume_;
               }
            }
         }
      }
   }

   BOOST_ASSERT(false);
   return {action::type::done, 0, system::error_code()};
}

}  // namespace boost::redis::detail
