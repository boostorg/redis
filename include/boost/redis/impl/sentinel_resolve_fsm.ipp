//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_SENTINEL_RESOLVE_FSM_IPP
#define BOOST_REDIS_SENTINEL_RESOLVE_FSM_IPP

#include <boost/redis/adapter/any_adapter.hpp>
#include <boost/redis/detail/connect_params.hpp>
#include <boost/redis/detail/connection_state.hpp>
#include <boost/redis/detail/coroutine.hpp>
#include <boost/redis/detail/sentinel_resolve_fsm.hpp>
#include <boost/redis/error.hpp>
#include <boost/redis/impl/is_terminal_cancel.hpp>
#include <boost/redis/impl/log_utils.hpp>
#include <boost/redis/impl/parse_sentinel_response.hpp>

#include <boost/asio/error.hpp>
#include <boost/assert.hpp>

#include <cstddef>
#include <random>

namespace boost::redis::detail {

inline void update_sentinel_list(
   std::vector<address>& to,
   std::size_t current_index,                      // the one to maintain and place first
   boost::span<const address> gossip_sentinels,    // the ones that SENTINEL SENTINELS returned
   boost::span<const address> bootstrap_sentinels  // the ones the user supplied
)
{
   // Place the one that succeeded in the front
   if (current_index != 0u)
      std::swap(to.front(), to[current_index]);

   // Remove the other Sentinels
   to.resize(1u);

   // Add one group
   to.insert(to.end(), gossip_sentinels.begin(), gossip_sentinels.end());

   // Insert any user-supplied sentinels, if not already present
   // TODO: maybe use a sorted vector?
   for (const auto& sentinel : bootstrap_sentinels) {
      auto it = std::find_if(to.begin(), to.end(), [&sentinel](const address& value) {
         return value.host == sentinel.host && value.port == sentinel.port;
      });
      if (it == to.end())
         to.push_back(sentinel);
   }
}

inline any_address_view current_sentinel(const connection_state& st, std::size_t idx)
{
   return any_address_view{st.sentinels[idx], st.cfg.sentinel.use_ssl};
}

// Returns how to connect to Sentinel given the current index
inline connect_params make_sentinel_connect_params(const connection_state& st, std::size_t idx)
{
   return {
      current_sentinel(st, idx),
      st.cfg.sentinel.resolve_timeout,
      st.cfg.sentinel.connect_timeout,
      st.cfg.sentinel.ssl_handshake_timeout,
   };
}

// TODO: adjust logging
sentinel_action sentinel_resolve_fsm::resume(
   connection_state& st,
   system::error_code ec,
   asio::cancellation_type_t cancel_state)
{
   switch (resume_point_) {
      BOOST_REDIS_CORO_INITIAL

      log_info(
         st.logger,
         "Trying to resolve the address of master '",
         st.cfg.sentinel.master_name,
         "' using Sentinel ",
         st.cfg.sentinel.use_ssl ? "(TLS enabled)" : "(TLS disabled)");

      // Try all Sentinels in order.
      // The following errors can be encountered for each Sentinel:
      //   1. Connecting to the Sentinel fails
      //   2. Executing the request fails with a network/parse error
      //   3. Executing the request fails with a server error
      //   4. Executing the request reports that Sentinel doesn't know about this master
      //   5. The user asked us to connect to a replica, but this master doesn't have any
      // We can only return one error code. If the user wants details, they can activate logging.
      // We perform the following translation:
      //   * If at least one Sentinel reported 5, no_replicas.
      //   * Otherwise, and if at least one Sentinel failed with 4, sentinel_unknown_master.
      //     This is likely a config error on the user side, and should be reported even if some Sentinels are offline.
      //   * Otherwise, no_sentinel_reachable. Details can be found in the logs.
      for (idx_ = 0u; idx_ < st.sentinels.size(); ++idx_) {
         log_debug(st.logger, "Trying to contact Sentinel at ", current_sentinel(st, idx_));

         // Try to connect
         BOOST_REDIS_YIELD(resume_point_, 1, make_sentinel_connect_params(st, idx_))

         // Check for cancellations
         if (is_terminal_cancel(cancel_state)) {
            log_debug(st.logger, "Sentinel resolve: cancelled (1)");
            return system::error_code(asio::error::operation_aborted);
         }

         // Check for errors
         if (ec) {
            log_info(
               st.logger,
               "Failed to connect to Sentinel at ",
               current_sentinel(st, idx_),
               ": ",
               ec);
            continue;
         }

         // Execute the Sentinel request
         log_debug(st.logger, "Executing Sentinel request at ", current_sentinel(st, idx_));
         st.sentinel_resp_nodes.clear();
         BOOST_REDIS_YIELD(resume_point_, 2, sentinel_action::request())

         // Check for cancellations
         if (is_terminal_cancel(cancel_state)) {
            log_debug(st.logger, "Sentinel resolve: cancelled (2)");
            st.sentinel_resp_nodes.clear();
            return system::error_code(asio::error::operation_aborted);
         }

         // Check for errors
         if (ec) {
            log_info(
               st.logger,
               "Failed to execute request at Sentinel ",
               current_sentinel(st, idx_),
               ": ",
               ec);
            st.sentinel_resp_nodes.clear();
            continue;
         }

         // Parse the response
         ec = parse_sentinel_response(
            st.sentinel_resp_nodes,
            st.cfg.sentinel.setup.get_expected_responses(),
            st.cfg.sentinel.server_role,
            st.sentinel_resp);

         // Reduce memory consumption. TODO: do we want to make these temporaries, instead?
         st.sentinel_resp_nodes.clear();

         if (ec) {
            if (ec == error::resp3_simple_error || ec == error::resp3_blob_error) {
               log_info(
                  st.logger,
                  "Sentinel response at ",
                  current_sentinel(st, idx_),
                  " contains an error: ",
                  st.sentinel_resp.diagnostic);
            } else if (ec == error::sentinel_unknown_master) {
               log_info(
                  st.logger,
                  "Sentinel ",
                  current_sentinel(st, idx_),
                  " doesn't know about master '",
                  st.cfg.sentinel.master_name,
                  "'");

               // This error code doesn't 'win' against no_replicas
               if (!final_ec_)
                  final_ec_ = error::sentinel_unknown_master;
            } else {
               log_info(
                  st.logger,
                  "Error parsing Sentinel response at ",
                  current_sentinel(st, idx_),
                  ": ",
                  ec);
            }

            continue;
         }

         // When asking for replicas, we might get no replicas.
         // This error code "wins" against any of the others.
         if (st.cfg.sentinel.server_role == role::replica && st.sentinel_resp.replicas.empty()) {
            log_info(
               st.logger,
               "Asked to connect to a replica, but Sentinel at ",
               current_sentinel(st, idx_),
               " indicates that the master has no replicas");
            final_ec_ = error::no_replicas;
            continue;
         }

         // Store the resulting address in a well-known place
         if (st.cfg.sentinel.server_role == role::master) {
            st.cfg.addr = st.sentinel_resp.master_addr;
         } else {
            // Choose a random replica
            std::uniform_int_distribution<std::size_t> dist{
               0u,
               st.sentinel_resp.replicas.size() - 1u};
            const auto idx = dist(st.eng);
            st.cfg.addr = st.sentinel_resp.replicas[idx];
         }

         // Sentinel knows about this master. Log and update our config
         log_info(
            st.logger,
            "Sentinel at ",
            current_sentinel(st, idx_),
            " resolved the address of master '",
            st.cfg.sentinel.master_name,
            "' to ",
            st.cfg.addr.host,
            ":",
            st.cfg.addr.port);

         update_sentinel_list(
            st.sentinels,
            idx_,
            st.sentinel_resp.sentinels,
            st.cfg.sentinel.addresses);

         return system::error_code();
      }

      // No Sentinel resolved our address. Adjust the error
      ec = final_ec_ ? final_ec_ : error::no_sentinel_reachable;
      log_err(
         st.logger,
         "Failed to resolve the address of master '",
         st.cfg.sentinel.master_name,
         "' using Sentinel: ",
         ec);

      return ec;
   }

   // We should never get here
   BOOST_ASSERT(false);
   return system::error_code();
}

// Note that we can't use generic_response because we need to tolerate error nodes.
any_adapter make_sentinel_adapter(connection_state& st)
{
   return make_vector_adapter(st.sentinel_resp_nodes);
}

}  // namespace boost::redis::detail

#endif
