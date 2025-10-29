//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/adapter/any_adapter.hpp>
#include <boost/redis/config.hpp>
#include <boost/redis/detail/connection_state.hpp>
#include <boost/redis/detail/coroutine.hpp>
#include <boost/redis/detail/multiplexer.hpp>
#include <boost/redis/detail/run_fsm.hpp>
#include <boost/redis/impl/is_terminal_cancel.hpp>
#include <boost/redis/impl/log_utils.hpp>
#include <boost/redis/impl/sentinel_adapter.hpp>
#include <boost/redis/impl/setup_request_utils.hpp>

#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/local/basic_endpoint.hpp>  // for BOOST_ASIO_HAS_LOCAL_SOCKETS
#include <boost/core/span.hpp>
#include <boost/system/error_code.hpp>

#include <cstddef>
#include <vector>

namespace boost::redis::detail {

inline system::error_code check_config(const config& cfg)
{
   if (!cfg.unix_socket.empty()) {
      if (cfg.use_ssl)
         return error::unix_sockets_ssl_unsupported;
      if (!cfg.sentinel.addresses.empty())
         return error::unix_sockets_sentinel_unsupported;
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

inline void process_setup_node(
   connection_state& st,
   resp3::basic_node<std::string_view> const& nd,
   system::error_code& ec)
{
   switch (nd.data_type) {
      case resp3::type::simple_error:
      case resp3::type::blob_error:
      case resp3::type::null:
         ec = redis::error::resp3_hello;
         st.setup_diagnostic = nd.value;
         break;
      default:;
   }
}

inline any_adapter make_setup_adapter(connection_state& st)
{
   return any_adapter{
      [&st](any_adapter::parse_event evt, resp3::node_view const& nd, system::error_code& ec) {
         if (evt == any_adapter::parse_event::node)
            process_setup_node(st, nd, ec);
      }};
}

inline void on_setup_done(const multiplexer::elem& elm, connection_state& st)
{
   const auto ec = elm.get_error();
   if (ec) {
      if (st.setup_diagnostic.empty()) {
         log_info(st.logger, "Setup request execution: ", ec);
      } else {
         log_info(st.logger, "Setup request execution: ", ec, " (", st.setup_diagnostic, ")");
      }
   } else {
      log_info(st.logger, "Setup request execution: success");
   }
}

// TODO
inline void update_sentinel_list(
   std::vector<address>& sentinels,
   std::size_t current_index,  // the one to maintain and place first
   boost::span<const address> other_sentinels);

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
         log_err(st.logger, "Invalid configuration: ", ec);
         stored_ec_ = ec;
         BOOST_REDIS_YIELD(resume_point_, 1, run_action_type::immediate)
         return stored_ec_;
      }

      // Compose the setup request. This only depends on the config, so it can be done just once
      compose_setup_request(st.cfg);

      // Compose the PING request. Same as above
      compose_ping_request(st.cfg, st.ping_req);

      for (;;) {
         // Sentinel connect
         if (!st.cfg.sentinel.addresses.empty()) {
            // Ask Sentinel where our server lives
            for (sentinel_idx_ = 0u; sentinel_idx_ < st.cfg.sentinel.addresses.size();
                 ++sentinel_idx_) {
               // Try to connect. TODO: we need a way to specify where and how to connect
               BOOST_REDIS_YIELD(resume_point_, 2, run_action_type::connect)

               // Check for cancellations
               if (is_terminal_cancel(cancel_state)) {
                  log_debug(st.logger, "Run: cancelled (10)");
                  return system::error_code(asio::error::operation_aborted);
               }

               // Check for errors
               if (ec) {
                  log_info(st.logger, "Failed to connect to Sentinel at <TODO>");
                  continue;
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
               update_sentinel_list(
                  st.cfg.sentinel.addresses,
                  sentinel_idx_,
                  st.sentinel_resp.sentinels);

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

         // Try to connect
         // TODO: this should be done differently for Sentinel
         BOOST_REDIS_YIELD(resume_point_, 20, run_action_type::connect)

         // Check for cancellations
         if (is_terminal_cancel(cancel_state)) {
            log_debug(st.logger, "Run: cancelled (1)");
            return system::error_code(asio::error::operation_aborted);
         }

         // If we were successful, run all the connection tasks
         if (!ec) {
            // Initialization
            st.mpx.reset();
            st.setup_diagnostic.clear();

            // Add the setup request to the multiplexer
            if (st.cfg.setup.get_commands() != 0u) {
               auto elm = make_elem(st.cfg.setup, make_setup_adapter(st));
               elm->set_done_callback([&elem_ref = *elm, &st] {
                  on_setup_done(elem_ref, st);
               });
               st.mpx.add(elm);
            }

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
            log_debug(st.logger, "Run: cancelled (2)");
            return system::error_code(asio::error::operation_aborted);
         }

         // When using Sentinel, we shouldn't wait here
         if (!st.cfg.sentinel.addresses.empty()) {
            // If we are not going to try again, we're done
            if (st.cfg.reconnect_wait_interval.count() == 0) {
               return ec;
            }

            // Wait for the reconnection interval
            BOOST_REDIS_YIELD(resume_point_, 5, run_action_type::wait_for_reconnection)

            // Check for cancellations
            if (is_terminal_cancel(cancel_state)) {
               log_debug(st.logger, "Run: cancelled (3)");
               return system::error_code(asio::error::operation_aborted);
            }
         }
      }
   }

   // We should never get here
   BOOST_ASSERT(false);
   return system::error_code();
}

any_adapter make_sentinel_adapter(const request& req, sentinel_response& resp)
{
   return any_adapter::impl_t([adapter = sentinel_adapter(req.get_expected_responses(), resp)](
                                 any_adapter::parse_event ev,
                                 resp3::node_view const& nd,
                                 system::error_code& ec) mutable {
      switch (ev) {
         case any_adapter::parse_event::init: adapter.on_init(); break;
         case any_adapter::parse_event::node: adapter.on_node(nd, ec); break;
         case any_adapter::parse_event::done: adapter.on_done(); break;
      }
   });
}

}  // namespace boost::redis::detail
