//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_SENTINEL_UTILS_HPP
#define BOOST_REDIS_SENTINEL_UTILS_HPP

#include <boost/redis/config.hpp>
#include <boost/redis/error.hpp>
#include <boost/redis/resp3/node.hpp>
#include <boost/redis/resp3/type.hpp>

#include <boost/assert.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/core/span.hpp>
#include <boost/system/error_code.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace boost::redis::detail {

// Returns true if Sentinel should be used
inline bool use_sentinel(const config& cfg) { return !cfg.sentinel.addresses.empty(); }

// Composes the request to send to Sentinel modifying cfg.sentinel.setup
inline void compose_sentinel_request(config& cfg)
{
   // These commands should go after the user-supplied setup, as this might involve authentication.
   // We ask for the master even when connecting to replicas to correctly detect when the master doesn't exist
   cfg.sentinel.setup.push("SENTINEL", "GET-MASTER-ADDR-BY-NAME", cfg.sentinel.master_name);
   if (cfg.sentinel.server_role == role::replica)
      cfg.sentinel.setup.push("SENTINEL", "REPLICAS", cfg.sentinel.master_name);
   cfg.sentinel.setup.push("SENTINEL", "SENTINELS", cfg.sentinel.master_name);

   // Note that we don't care about request flags because this is a one-time request
}

// Parses a list of replicas or sentinels
inline system::error_code parse_server_list(
   const resp3::node*& first,
   const resp3::node* last,
   std::vector<address>& out)
{
   const auto* it = first;
   ignore_unused(last);

   // The root node must be an array
   BOOST_ASSERT(it != last);
   BOOST_ASSERT(it->depth == 0u);
   if (it->data_type != resp3::type::array)
      return {error::invalid_data_type};
   const std::size_t num_servers = it->aggregate_size;
   ++it;

   // Each element in the array represents a server
   out.resize(num_servers);
   for (std::size_t i = 0u; i < num_servers; ++i) {
      // A server is a map (resp3) or array (resp2, currently unsupported)
      BOOST_ASSERT(it != last);
      BOOST_ASSERT(it->depth == 1u);
      if (it->data_type != resp3::type::map)
         return {error::invalid_data_type};
      const std::size_t num_key_values = it->aggregate_size;
      ++it;

      // The server object is composed by a set of key/value pairs.
      // Skip everything except for the ones we care for.
      bool ip_seen = false, port_seen = false;
      for (std::size_t j = 0; j < num_key_values; ++j) {
         // Key. It should be a string
         BOOST_ASSERT(it != last);
         BOOST_ASSERT(it->depth == 2u);
         if (it->data_type != resp3::type::blob_string)
            return {error::invalid_data_type};
         const std::string_view key = it->value;
         ++it;

         // Value. All values seem to be strings, too.
         BOOST_ASSERT(it != last);
         BOOST_ASSERT(it->depth == 2u);
         if (it->data_type != resp3::type::blob_string)
            return {error::invalid_data_type};

         // Record it
         if (key == "ip") {
            ip_seen = true;
            out[i].host = it->value;
         } else if (key == "port") {
            port_seen = true;
            out[i].port = it->value;
         }

         ++it;
      }

      // Check that the response actually contained the fields we wanted
      if (!ip_seen || !port_seen)
         return {error::empty_field};
   }

   // Done
   first = it;
   return system::error_code();
}

// The output type of parse_sentinel_response
struct sentinel_response {
   std::string diagnostic;         // In case the server returned an error
   address master_addr;            // Always populated
   std::vector<address> replicas;  // Populated only when connecting to replicas
   std::vector<address> sentinels;
};

// Parses an array of nodes into a sentinel_response.
// The request originating this response should be:
//    <user-supplied commands, as per sentinel_config::setup>
//    SENTINEL GET-MASTER-ADDR-BY-NAME
//    SENTINEL REPLICAS (only if server_role is replica)
//    SENTINEL SENTINELS
// SENTINEL SENTINELS and SENTINEL REPLICAS error when the master name is unknown. Error nodes
// should be allowed in the node array.
// This means that we can't use generic_response, since its adapter errors on error nodes.
// SENTINEL GET-MASTER-ADDR-BY-NAME is sent even when connecting to replicas
//    for better diagnostics when the master name is unknown.
// Preconditions:
//   * There are at least 2 (master)/3 (replica) root nodes.
//   * The node array originates from parsing a valid RESP3 message.
//     E.g. we won't check that the first node has depth 0.
inline system::error_code parse_sentinel_response(
   span<const resp3::node> nodes,
   role server_role,
   sentinel_response& out)
{
   auto check_errors = [&out](const resp3::node& nd) {
      switch (nd.data_type) {
         case resp3::type::simple_error:
            out.diagnostic = nd.value;
            return system::error_code(error::resp3_simple_error);
         case resp3::type::blob_error:
            out.diagnostic = nd.value;
            return system::error_code(error::resp3_blob_error);
         default: return system::error_code();
      }
   };

   // Clear the output
   out.diagnostic.clear();
   out.sentinels.clear();
   out.replicas.clear();

   // Find the first root node of interest. It's the 2nd or 3rd, starting with the end
   auto find_first = [nodes, server_role] {
      const std::size_t expected_roots = server_role == role::master ? 2u : 3u;
      std::size_t roots_seen = 0u;
      for (auto it = nodes.rbegin();; ++it) {
         BOOST_ASSERT(it != nodes.rend());
         if (it->depth == 0u && ++roots_seen == expected_roots)
            return &*it;
      }
   };
   const resp3::node* lib_first = find_first();

   // Iterators
   const resp3::node* it = nodes.begin();
   const resp3::node* last = nodes.end();
   ignore_unused(last);

   // Go through all the responses to user-supplied requests checking for errors
   for (; it != lib_first; ++it) {
      if (auto ec = check_errors(*it))
         return ec;
   }

   // SENTINEL GET-MASTER-ADDR-BY-NAME

   // Check for errors
   if (auto ec = check_errors(*it))
      return ec;

   // If the root node is NULL, Sentinel doesn't know about this master.
   // We use resp3_null to signal this fact. This doesn't reach the end user.
   if (it->data_type == resp3::type::null) {
      return {error::resp3_null};
   }

   // If the root node is an array, an IP and port follow
   if (it->data_type != resp3::type::array)
      return {error::invalid_data_type};
   if (it->aggregate_size != 2u)
      return {error::incompatible_size};
   ++it;

   // IP
   BOOST_ASSERT(it != last);
   BOOST_ASSERT(it->depth == 1u);
   if (it->data_type != resp3::type::blob_string)
      return {error::invalid_data_type};
   out.master_addr.host = it->value;
   ++it;

   // Port
   BOOST_ASSERT(it != last);
   BOOST_ASSERT(it->depth == 1u);
   if (it->data_type != resp3::type::blob_string)
      return {error::invalid_data_type};
   out.master_addr.port = it->value;
   ++it;

   if (server_role == role::replica) {
      // SENTINEL REPLICAS

      // This request fails if Sentinel doesn't know about this master.
      // However, that's not the case if we got here.
      // Check for other errors.
      if (auto ec = check_errors(*it))
         return ec;

      // Actual parsing
      if (auto ec = parse_server_list(it, last, out.replicas))
         return ec;
   }

   // SENTINEL SENTINELS

   // This request fails if Sentinel doesn't know about this master.
   // However, that's not the case if we got here.
   // Check for other errors.
   if (auto ec = check_errors(*it))
      return ec;

   // Actual parsing
   if (auto ec = parse_server_list(it, last, out.sentinels))
      return ec;

   // Done
   return system::error_code();
}

// Updates the internal Sentinel list.
// to should never be empty
inline void update_sentinel_list(
   std::vector<address>& to,
   std::size_t current_index,               // the one to maintain and place first
   span<const address> gossip_sentinels,    // the ones that SENTINEL SENTINELS returned
   span<const address> bootstrap_sentinels  // the ones the user supplied
)
{
   BOOST_ASSERT(!to.empty());

   // Remove everything, except the Sentinel that succeeded
   if (current_index != 0u)
      std::swap(to.front(), to[current_index]);
   to.resize(1u);

   // Add one group. These Sentinels are always unique and don't include the one we're currently connected to.
   to.insert(to.end(), gossip_sentinels.begin(), gossip_sentinels.end());

   // Insert any user-supplied sentinels, if not already present.
   // This is O(n^2), but is okay because n will be small.
   // The list can't be sorted, anyway
   for (const auto& sentinel : bootstrap_sentinels) {
      if (std::find(to.begin(), to.end(), sentinel) == to.end())
         to.push_back(sentinel);
   }
}

}  // namespace boost::redis::detail

#endif
