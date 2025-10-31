//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_SENTINEL_RESOLVE_FSM_IPP
#define BOOST_REDIS_SENTINEL_RESOLVE_FSM_IPP

#include <boost/redis/detail/connect_params.hpp>
#include <boost/redis/detail/connection_state.hpp>
#include <boost/redis/detail/coroutine.hpp>
#include <boost/redis/detail/sentinel_resolve_fsm.hpp>
#include <boost/redis/error.hpp>
#include <boost/redis/impl/is_terminal_cancel.hpp>
#include <boost/redis/impl/log_utils.hpp>
#include <boost/redis/impl/sentinel_adapter.hpp>

#include <boost/asio/error.hpp>
#include <boost/assert.hpp>

#include <cstddef>

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

sentinel_action sentinel_resolve_fsm::resume(
   connection_state& st,
   system::error_code ec,
   asio::cancellation_type_t cancel_state)
{
   switch (resume_point_) {
      BOOST_REDIS_CORO_INITIAL

      // Try all Sentinels in order.
      // The following errors can be encountered for each Sentinel:
      //   1. Connecting to the Sentinel fails
      //   2. Executing the request fails with a network/parse error
      //   3. Executing the request fails with a server error
      //   4. Executing the request reports that Sentinel doesn't know about this master
      // We can only return one error code. If the user wants details, they can activate logging.
      // We perform the following translation:
      //   * If at least one Sentinel failed with 4, sentinel_unknown_master.
      //     This is likely a config error on the user side, and should be reported even if some Sentinels are offline.
      //   * Otherwise, no_sentinel_reachable. Details can be found in the logs.
      for (idx_ = 0u; idx_ < st.sentinels.size(); ++idx_) {
         log_info(st.logger, "Trying to contact Sentinel at ", current_sentinel(st, idx_));

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
         BOOST_REDIS_YIELD(resume_point_, 2, sentinel_action::request())

         // Check for cancellations
         if (is_terminal_cancel(cancel_state)) {
            log_debug(st.logger, "Sentinel resolve: cancelled (2)");
            return system::error_code(asio::error::operation_aborted);
         }

         // Check for errors
         if (ec) {
            log_info(
               st.logger,
               "Failed to execute request at Sentinel ",
               current_sentinel(st, idx_),
               st.sentinel_resp.diagnostic.empty() ? "" : ": ",
               st.sentinel_resp.diagnostic);

            // If the error indicated an unknown master, record it to adjust the final error
            if (ec == error::sentinel_unknown_master)
               unknown_master_ = true;

            continue;
         }

         // Sentinel knows about this master. Log and update our config
         log_info(
            st.logger,
            "Sentinel at ",
            current_sentinel(st, idx_),
            " returned address ",
            st.sentinel_resp.server_addr.host,
            ":",
            st.sentinel_resp.server_addr.port);

         update_sentinel_list(
            st.sentinels,
            idx_,
            st.sentinel_resp.sentinels,
            st.cfg.sentinel.addresses);

         return system::error_code();
      }

      // No Sentinel available
      return system::error_code{
         unknown_master_ ? error::sentinel_unknown_master : error::no_sentinel_reachable};
   }

   // We should never get here
   BOOST_ASSERT(false);
   return system::error_code();
}

any_adapter make_sentinel_adapter(connection_state& st)
{
   return any_adapter::impl_t([adapter = sentinel_adapter(
                                  st.cfg.sentinel.setup.get_expected_responses(),
                                  st.sentinel_resp)](
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

#endif
