//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_SENTINEL_RESOLVE_FSM_IPP
#define BOOST_REDIS_SENTINEL_RESOLVE_FSM_IPP

#include <boost/redis/detail/connection_state.hpp>
#include <boost/redis/detail/coroutine.hpp>
#include <boost/redis/detail/sentinel_resolve_fsm.hpp>
#include <boost/redis/impl/is_terminal_cancel.hpp>
#include <boost/redis/impl/log_utils.hpp>

#include <boost/asio/error.hpp>
#include <boost/assert.hpp>

namespace boost::redis::detail {

sentinel_action sentinel_resolve_fsm::resume(
   connection_state& st,
   system::error_code ec,
   asio::cancellation_type_t cancel_state)
{
   switch (resume_point_) {
      BOOST_REDIS_CORO_INITIAL

      // Ask Sentinel where our server lives
      for (idx_ = 0u; idx_ < st.cfg.sentinel.addresses.size(); ++idx_) {
         // Try to connect
         BOOST_REDIS_YIELD(
            resume_point_,
            1,
            sentinel_action::connect(st.cfg.sentinel.addresses[idx_]))

         // Check for cancellations
         if (is_terminal_cancel(cancel_state)) {
            log_debug(st.logger, "Sentinel resolve: cancelled (1)");
            return system::error_code(asio::error::operation_aborted);
         }

         // Check for errors
         if (ec) {
            log_info(st.logger, "Failed to connect to Sentinel at <TODO>");
            continue;
         }

         // Write the Sentinel request
         BOOST_REDIS_YIELD(resume_point_, 2, sentinel_action::write(st.sentinel_req.payload()))

         // Check for cancellations
         if (is_terminal_cancel(cancel_state)) {
            log_debug(st.logger, "Sentinel resolve: cancelled (2)");
            return system::error_code(asio::error::operation_aborted);
         }

         // Check for errors
         if (ec) {
            log_info(st.logger, "Failed to write Sentinel request at <TODO>");
            continue;
         }

         // Read Sentinel's response
         st.mpx.get_read_buffer().clear();

         while (true) {
            ec = st.mpx.get_read_buffer().prepare();
            if (ec) {
               log_info(st.logger, "Error preparing buffer for Sentinel read operation: ", ec);
               continue;
            }

            BOOST_REDIS_YIELD(resume_point_, 3, sentinel_action::read())

            // Check for cancellations
            if (is_terminal_cancel(cancel_state)) {
               log_debug(st.logger, "Sentinel resolve: cancelled (3)");
               return system::error_code(asio::error::operation_aborted);
            }

            // Check for errors
            if (ec) {
               log_info(st.logger, "Failed to read Sentinel response at <TODO>: ", ec);
               continue;
            }

            // Feed it to the parser
         }

         // Execute the Sentinel request
         BOOST_REDIS_YIELD(resume_point_, 35, run_action_type::sentinel_request)

         // Check for cancellations
         if (is_terminal_cancel(cancel_state)) {
            log_debug(st.logger, "Run: cancelled (11)");
            return system::error_code(asio::error::operation_aborted);
         }

         // Check for errors
         // TODO: these diagnostics are not good
         if (ec) {
            log_info(st.logger, "Failed to execute Sentinel request for <TODO>");
            continue;
         }

         // Sentinel knows about this master. Update our config
         update_sentinel_list(st.cfg.sentinel.addresses, sentinel_idx_, st.sentinel_resp.sentinels);

         break;
      }

      // TODO: is this check reliable?
      // TODO: this diagnostic is not good
      if (ec) {
         log_err(st.logger, "No Sentinel can be reached");

         // If we are not going to try again, we're done
         if (st.cfg.reconnect_wait_interval.count() == 0) {
            return ec;
         }

         // Wait for the reconnection interval.
         // This is not technically what Redis docs recommends,
         // but I think it's consistent with what non-sentinel run does
         BOOST_REDIS_YIELD(resume_point_, 55, run_action_type::wait_for_reconnection)

         // Check for cancellations
         if (is_terminal_cancel(cancel_state)) {
            log_debug(st.logger, "Run: cancelled (35)");
            return system::error_code(asio::error::operation_aborted);
         }

         // Try again
         continue;
      }
   }

   // We should never get here
   BOOST_ASSERT(false);
   return system::error_code();
}

}  // namespace boost::redis::detail

#endif
