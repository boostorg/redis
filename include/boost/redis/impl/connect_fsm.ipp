//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/config.hpp>
#include <boost/redis/detail/connect_fsm.hpp>
#include <boost/redis/detail/connection_logger.hpp>
#include <boost/redis/detail/coroutine.hpp>
#include <boost/redis/error.hpp>

#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/error.hpp>
#include <boost/assert.hpp>

namespace boost::redis::detail {

inline transport_type transport_from_config(const config& cfg)
{
   if (cfg.unix_socket.empty()) {
      if (cfg.use_ssl) {
         return transport_type::tcp_tls;
      } else {
         return transport_type::tcp;
      }
   } else {
      BOOST_ASSERT(!cfg.use_ssl);
      return transport_type::unix_socket;
   }
}

inline system::error_code translate_timeout_error(
   system::error_code io_ec,
   asio::cancellation_type_t cancel_state,
   error code_if_cancelled)
{
   // Translates cancellations and timeout errors into a single error_code.
   //   - Cancellation state set, and an I/O error: the entire operation was cancelled.
   //     The I/O code (probably operation_aborted) is appropriate.
   //   - Cancellation state set, and no I/O error: same as above, but the cancellation
   //     arrived after the operation completed and before the handler was called. Set the code here.
   //   - No cancellation state set, I/O error set to operation_aborted: since we use cancel_after,
   //     this means a timeout.
   //   - Otherwise, respect the I/O error.
   if ((cancel_state & asio::cancellation_type_t::terminal) != asio::cancellation_type_t::none) {
      return io_ec ? io_ec : asio::error::operation_aborted;
   }
   return io_ec == asio::error::operation_aborted ? code_if_cancelled : io_ec;
}

connect_action connect_fsm::resume(
   system::error_code ec,
   const asio::ip::tcp::resolver::results_type& resolver_results,
   redis_stream_state& st,
   asio::cancellation_type_t cancel_state)
{
   // Translate error codes
   ec = translate_timeout_error(ec, cancel_state, error::resolve_timeout);

   // Log it
   lgr_->on_resolve(ec, resolver_results);

   // Delegate to the regular resume function
   return resume(ec, st, cancel_state);
}

connect_action connect_fsm::resume(
   system::error_code ec,
   const asio::ip::tcp::endpoint& selected_endpoint,
   redis_stream_state& st,
   asio::cancellation_type_t cancel_state)
{
   // Translate error codes
   ec = translate_timeout_error(ec, cancel_state, error::connect_timeout);

   // Log it
   lgr_->on_connect(ec, selected_endpoint);

   // Delegate to the regular resume function
   return resume(ec, st, cancel_state);
}

connect_action connect_fsm::resume(
   system::error_code ec,
   redis_stream_state& st,
   asio::cancellation_type_t cancel_state)
{
   switch (resume_point_) {
      BOOST_REDIS_CORO_INITIAL

      // Record the transport that we will be using
      st.type = transport_from_config(*cfg_);

      if (st.type == transport_type::unix_socket) {
         // Reset the socket, to discard any previous state. Ignore any errors
         BOOST_REDIS_YIELD(resume_point_, 1, connect_action_type::unix_socket_close)

         // Connect to the socket
         BOOST_REDIS_YIELD(resume_point_, 2, connect_action_type::unix_socket_connect)

         // Fix error codes. If we were cancelled and the code is operation_aborted,
         // it is because per-operation cancellation was activated. If we were not cancelled
         // but the operation failed with operation_aborted, it's a timeout.
         // Also check for cancellations that didn't cause a failure
         ec = translate_timeout_error(ec, cancel_state, error::connect_timeout);

         // Log it
         lgr_->on_connect(ec, cfg_->unix_socket);

         // If this failed, we can't continue
         if (ec) {
            return ec;
         }

         // Done
         return system::error_code();
      } else {
         // ssl::stream doesn't support being re-used. If we're to use
         // TLS and the stream has been used, re-create it.
         // Must be done before anything else is done on the stream.
         // We don't need to close the TCP socket if using plaintext TCP
         // because range-connect closes open sockets, while individual connect doesn't
         if (cfg_->use_ssl && st.ssl_stream_used) {
            BOOST_REDIS_YIELD(resume_point_, 3, connect_action_type::ssl_stream_reset)
         }

         // Resolve names. The continuation needs access to the returned
         // endpoints, and is a specialized resume() that will call this function
         BOOST_REDIS_YIELD(resume_point_, 4, connect_action_type::tcp_resolve)

         // If this failed, we can't continue (error code translation already performed here)
         if (ec) {
            return ec;
         }

         // Now connect to the endpoints returned by the resolver.
         // This has a specialized resume(), too
         BOOST_REDIS_YIELD(resume_point_, 5, connect_action_type::tcp_connect)

         // If this failed, we can't continue (error code translation already performed here)
         if (ec) {
            return ec;
         }

         if (cfg_->use_ssl) {
            // Mark the SSL stream as used
            st.ssl_stream_used = true;

            // Perform the TLS handshake
            BOOST_REDIS_YIELD(resume_point_, 6, connect_action_type::ssl_handshake)

            // Translate error codes
            ec = translate_timeout_error(ec, cancel_state, error::ssl_handshake_timeout);

            // Log it
            lgr_->on_ssl_handshake(ec);

            // If this failed, we can't continue
            if (ec) {
               return ec;
            }
         }

         // Done
         return system::error_code();
      }
   }

   BOOST_ASSERT(false);
   return system::error_code();
}

}  // namespace boost::redis::detail
