/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_SETUP_REQUEST_UTILS_HPP
#define BOOST_REDIS_SETUP_REQUEST_UTILS_HPP

#include <boost/redis/config.hpp>
#include <boost/redis/detail/connection_state.hpp>
#include <boost/redis/error.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/resp3/node.hpp>
#include <boost/redis/resp3/type.hpp>
#include <boost/redis/response.hpp>

#include <cstddef>

namespace boost::redis::detail {

// Not strictly related to setup, but used across .ipp files
inline bool use_sentinel(const config& cfg) { return !cfg.sentinel.addresses.empty(); }

// Modifies config::setup to make a request suitable to be sent
// to the server using async_exec
inline void compose_setup_request(config& cfg)
{
   auto& req = cfg.setup;

   if (!cfg.use_setup) {
      // We're not using the setup request as-is, but should compose one based on
      // the values passed by the user
      req.clear();

      // Which parts of the command should we send?
      // Don't send AUTH if the user is the default and the password is empty.
      // Other users may have empty passwords.
      // Note that this is just an optimization.
      bool send_auth = !(
         cfg.username.empty() || (cfg.username == "default" && cfg.password.empty()));
      bool send_setname = !cfg.clientname.empty();

      // Gather everything we can in a HELLO command
      if (send_auth && send_setname)
         req.push("HELLO", "3", "AUTH", cfg.username, cfg.password, "SETNAME", cfg.clientname);
      else if (send_auth)
         req.push("HELLO", "3", "AUTH", cfg.username, cfg.password);
      else if (send_setname)
         req.push("HELLO", "3", "SETNAME", cfg.clientname);
      else
         req.push("HELLO", "3");

      // SELECT is independent of HELLO
      if (cfg.database_index && cfg.database_index.value() != 0)
         req.push("SELECT", cfg.database_index.value());
   }

   // When using Sentinel, we should add a role check.
   // This must happen after the other commands, as it requires authentication.
   if (use_sentinel(cfg))
      req.push("ROLE");

   // In any case, the setup request should have the priority
   // flag set so it's executed before any other request.
   // The setup request should never be retried.
   request_access::set_priority(req, true);
   req.get_config().cancel_if_unresponded = true;
   req.get_config().cancel_on_connection_lost = true;
}

class setup_adapter {
   connection_state* st_;
   std::size_t response_idx_{0u};
   bool role_seen_{false};

   system::error_code on_node_impl(const resp3::node_view& nd)
   {
      // An error node is always an error
      switch (nd.data_type) {
         case resp3::type::simple_error:
         case resp3::type::blob_error:   st_->setup_diagnostic = nd.value; return error::resp3_hello;
         default:                        ;
      }

      // When using Sentinel, we add a ROLE command at the end.
      // We need to ensure that this instance is a master.
      if (use_sentinel(st_->cfg) && response_idx_ == st_->cfg.setup.get_expected_responses() - 1u) {
         // ROLE's response should be an array
         if (nd.depth == 0u) {
            if (nd.data_type != resp3::type::array)
               return error::invalid_data_type;
         }

         // The first node should be 'master' if we're connecting to a primary,
         // 'slave' if we're connecting to a replica
         if (nd.depth == 1u && !role_seen_) {
            role_seen_ = true;
            const char* expected_role = st_->cfg.sentinel.server_role == role::master ? "master"
                                                                                      : "slave";
            if (nd.value != expected_role) {
               return error::role_check_failed;
            }
         }
      }

      return system::error_code();
   }

public:
   explicit setup_adapter(connection_state& st) noexcept
   : st_(&st)
   { }

   void on_init() { }
   void on_done() { ++response_idx_; }
   void on_node(const resp3::node_view& node, system::error_code& ec) { ec = on_node_impl(node); }
};

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_RUNNER_HPP
