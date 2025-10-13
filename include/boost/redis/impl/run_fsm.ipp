//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/config.hpp>
#include <boost/redis/detail/coroutine.hpp>
#include <boost/redis/detail/run_fsm.hpp>
#include <boost/redis/detail/setup_request_utils.hpp>
#include <boost/redis/impl/is_terminal_cancel.hpp>

#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/local/basic_endpoint.hpp>  // for BOOST_ASIO_HAS_LOCAL_SOCKETS
#include <boost/system/error_code.hpp>

namespace boost::redis::detail {

inline system::error_code check_config(const config& cfg)
{
   if (!cfg.unix_socket.empty()) {
      if (cfg.use_ssl)
         return error::unix_sockets_ssl_unsupported;
#ifndef BOOST_ASIO_HAS_LOCAL_SOCKETS
      return error::unix_sockets_unsupported;
#endif
   }
   return system::error_code{};
}

inline void compose_ping_request(const config& cfg, request& to)
{
   to.clear();
   to.push("PING", cfg.health_check_id);
}

run_action run_fsm::resume(
   connection_state& st,
   system::error_code ec,
   asio::cancellation_type_t cancel_state)
{
   switch (resume_point_) {
      BOOST_REDIS_CORO_INITIAL

      // Check config
      ec = check_config(st.cfg);
      if (ec) {
         st.logger.log(logger::level::err, "Invalid configuration", ec);
         stored_ec_ = ec;
         BOOST_REDIS_YIELD(resume_point_, 1, run_action_type::immediate)
         return stored_ec_;
      }

      // Compose the setup request. This only depends on the config, so it can be done just once
      compose_setup_request(st.cfg);

      // Compose the PING request. Same as above
      compose_ping_request(st.cfg, st.ping_req);

      for (;;) {
         // Try to connect
         BOOST_REDIS_YIELD(resume_point_, 2, run_action_type::connect)

         // Check for cancellations
         if (is_terminal_cancel(cancel_state)) {
            st.logger.trace("Run: cancelled (1)");
            return system::error_code(asio::error::operation_aborted);
         }

         // If we were successful, run all the connection tasks
         if (!ec) {
            // Initialization
            st.mpx.reset();
            clear_response(st.setup_resp);

            // Run the tasks
            BOOST_REDIS_YIELD(resume_point_, 3, run_action_type::parallel_group)

            // Store any error yielded by the tasks for later
            stored_ec_ = ec;

            // We've lost connection or otherwise been cancelled.
            // Remove from the multiplexer the required requests.
            st.mpx.cancel_on_conn_lost();

            // The receive operation must be cancelled because channel
            // subscription does not survive a reconnection but requires
            // re-subscription.
            BOOST_REDIS_YIELD(resume_point_, 4, run_action_type::cancel_receive)

            // Restore the error
            ec = stored_ec_;
         }

         // Check for cancellations
         if (is_terminal_cancel(cancel_state)) {
            st.logger.trace("Run: cancelled (2)");
            return system::error_code(asio::error::operation_aborted);
         }

         // If we are not going to try again, we're done
         if (st.cfg.reconnect_wait_interval.count() == 0) {
            return ec;
         }

         // Wait for the reconnection interval
         BOOST_REDIS_YIELD(resume_point_, 5, run_action_type::wait_for_reconnection)

         // Check for cancellations
         if (is_terminal_cancel(cancel_state)) {
            st.logger.trace("Run: cancelled (3)");
            return system::error_code(asio::error::operation_aborted);
         }
      }
   }

   // We should never get here
   BOOST_ASSERT(false);
   return system::error_code();
}

}  // namespace boost::redis::detail
