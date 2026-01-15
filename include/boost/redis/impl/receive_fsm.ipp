//
// Copyright (c) 2018-2026 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/detail/connection_state.hpp>
#include <boost/redis/detail/coroutine.hpp>
#include <boost/redis/detail/receive_fsm.hpp>

#include <boost/asio/error.hpp>
#include <boost/asio/experimental/channel_error.hpp>
#include <boost/assert.hpp>
#include <boost/system/detail/error_code.hpp>

namespace boost::redis::detail {

constexpr bool is_any_cancel(asio::cancellation_type_t type)
{
   return !!(
      type & (asio::cancellation_type_t::terminal | asio::cancellation_type_t::partial |
              asio::cancellation_type_t::total));
}

receive_action receive_fsm::resume(
   connection_state& st,
   system::error_code ec,
   asio::cancellation_type_t cancel_state)
{
   switch (resume_point_) {
      BOOST_REDIS_CORO_INITIAL

      // This operation supports total cancellation. Set it up
      BOOST_REDIS_YIELD(resume_point_, 1, receive_action::action_type::setup_cancellation)

      while (true) {
         // Wait at least once for a notification to arrive
         BOOST_REDIS_YIELD(resume_point_, 2, receive_action::action_type::wait)

         // If the wait completed successfully, we have pushes. Drain the channel and exit
         if (!ec) {
            BOOST_REDIS_YIELD(resume_point_, 3, receive_action::action_type::drain_channel)
            return system::error_code();
         }

         // Check for cancellations
         if (is_any_cancel(cancel_state))
            return system::error_code(asio::error::operation_aborted);

         // If the channel was cancelled, it might be due to a reconnection.
         // If the connection isn't reconnecting (run is exiting), exit, otherwise continue.
         if (ec == asio::experimental::channel_errc::channel_cancelled) {
            if (st.cfg.reconnect_wait_interval.count() == 0) {
               // Won't reconnect
               return system::error_code(asio::error::operation_aborted);
            } else {
               // Will reconnect, ignore the notification
               continue;
            }
         } else {
            // This is an unknown error. Propagate it, just in case
            return ec;
         }
      }
   }

   // We should never get here
   BOOST_ASSERT(false);
   return receive_action{system::error_code()};
}

}  // namespace boost::redis::detail
