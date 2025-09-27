//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_CONNECT_FSM_HPP
#define BOOST_REDIS_CONNECT_FSM_HPP

#include <boost/redis/config.hpp>
#include <boost/redis/detail/connection_logger.hpp>
#include <boost/redis/detail/coroutine.hpp>
#include <boost/redis/error.hpp>

#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/assert.hpp>
#include <boost/system/error_code.hpp>

// Sans-io algorithm for redis_stream::async_connect, as a finite state machine

namespace boost::redis::detail {

// TODO: this is duplicated
inline bool is_terminal_cancellation(asio::cancellation_type_t value)
{
   return (value & asio::cancellation_type_t::terminal) != asio::cancellation_type_t::none;
}

// What transport is redis_stream using?
enum class transport_type
{
   tcp,          // plaintext TCP
   tcp_tls,      // TLS over TCP
   unix_socket,  // UNIX domain sockets
};

struct redis_stream_state {
   transport_type type{transport_type::tcp};
   bool ssl_stream_used{false};
};

// What should we do next?
enum class connect_action_type
{
   unix_socket_connect,
   tcp_resolve,
   tcp_connect,
   ssl_stream_reset,
   ssl_handshake,
   done,
};

class connect_action {
   connect_action_type type_;
   system::error_code ec_;

public:
   connect_action(connect_action_type type) noexcept
   : type_{type}
   { }

   connect_action(system::error_code ec) noexcept
   : type_{connect_action_type::done}
   , ec_{ec}
   { }

   connect_action_type type() const { return type_; }
   system::error_code error() const { return ec_; }
};

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

class connect_fsm {
   int resume_point_{0};
   const config* cfg_{nullptr};
   connection_logger* lgr_{nullptr};

public:
   connect_fsm(const config& cfg, connection_logger& lgr) noexcept
   : cfg_(&cfg)
   , lgr_(&lgr)
   { }

   const config& get_config() const { return *cfg_; }

   connect_action resume(
      system::error_code ec,
      const asio::ip::tcp::resolver::results_type& resolver_results,
      redis_stream_state& st,
      asio::cancellation_type_t cancel_state)
   {
      // Translate error codes
      if (is_terminal_cancellation(cancel_state)) {
         if (ec) {
            if (ec == asio::error::operation_aborted) {
               ec = error::resolve_timeout;
            }
         } else {
            ec = asio::error::operation_aborted;
         }
      }

      // Log it
      lgr_->on_resolve(ec, resolver_results);

      // Delegate to the regular resume function
      return resume(ec, st, cancel_state);
   }

   connect_action resume(
      system::error_code ec,
      const asio::ip::tcp::endpoint& selected_endpoint,
      redis_stream_state& st,
      asio::cancellation_type_t cancel_state)
   {
      // Translate error codes
      if (is_terminal_cancellation(cancel_state)) {
         if (ec) {
            if (ec == asio::error::operation_aborted) {
               ec = error::connect_timeout;
            }
         } else {
            ec = asio::error::operation_aborted;
         }
      }

      // Log it
      lgr_->on_connect(ec, selected_endpoint);

      // Delegate to the regular resume function
      return resume(ec, st, cancel_state);
   }

   connect_action resume(
      system::error_code ec,
      redis_stream_state& st,
      asio::cancellation_type_t cancel_state)
   {
      switch (resume_point_) {
         BOOST_REDIS_CORO_INITIAL

         // Record the transport that we will be using
         st.type = transport_from_config(*cfg_);

         if (st.type == transport_type::unix_socket) {
            // Directly connect to the socket
            BOOST_REDIS_YIELD(resume_point_, 1, connect_action_type::unix_socket_connect)

            // Fix error codes. If we were cancelled and the code is operation_aborted,
            // it is because per-operation cancellation was activated. If we were not cancelled
            // but the operation failed with operation_aborted, it's a timeout.
            // Also check for cancellations that didn't cause a failure
            if (is_terminal_cancellation(cancel_state)) {
               if (ec) {
                  if (ec == asio::error::operation_aborted) {
                     ec = error::connect_timeout;
                  }
               } else {
                  ec = asio::error::operation_aborted;
               }
            }

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
            // Must be done before anything else is done on the stream
            if (cfg_->use_ssl && st.ssl_stream_used) {
               BOOST_REDIS_YIELD(resume_point_, 2, connect_action_type::ssl_stream_reset)
            }

            // Resolve names. The continuation needs access to the returned
            // endpoints, and is a specialized resume() that will call this function
            BOOST_REDIS_YIELD(resume_point_, 3, connect_action_type::tcp_resolve)

            // If this failed, we can't continue (error code translation already performed here)
            if (ec) {
               return ec;
            }

            // Now connect to the endpoints returned by the resolver.
            // This has a specialized resume(), too
            BOOST_REDIS_YIELD(resume_point_, 4, connect_action_type::tcp_connect)

            // If this failed, we can't continue (error code translation already performed here)
            if (ec) {
               return ec;
            }

            if (cfg_->use_ssl) {
               // Mark the SSL stream as used
               st.ssl_stream_used = true;

               // Perform the TLS handshake
               BOOST_REDIS_YIELD(resume_point_, 5, connect_action_type::ssl_handshake)

               // Translate error codes
               if (is_terminal_cancellation(cancel_state)) {
                  if (ec) {
                     if (ec == asio::error::operation_aborted) {
                        ec = error::ssl_handshake_timeout;
                     }
                  } else {
                     ec = asio::error::operation_aborted;
                  }
               }

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

};  // namespace boost::redis::detail

}  // namespace boost::redis::detail

#endif
