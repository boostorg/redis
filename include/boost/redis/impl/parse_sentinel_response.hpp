//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_PARSE_SENTINEL_RESPONSE_HPP
#define BOOST_REDIS_PARSE_SENTINEL_RESPONSE_HPP

#include <boost/redis/config.hpp>
#include <boost/redis/detail/connection_state.hpp>
#include <boost/redis/error.hpp>
#include <boost/redis/resp3/node.hpp>
#include <boost/redis/resp3/type.hpp>

#include <boost/assert.hpp>
#include <boost/core/span.hpp>
#include <boost/system/error_code.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace boost::redis::detail {

// Parses a list of replicas or sentinels
inline system::error_code parse_server_list(
   const resp3::node*& first,
   const resp3::node* last,
   std::vector<address>& out)
{
   const auto* it = first;

   // The root node must be an array
   BOOST_ASSERT(it != last);
   if (it->depth != 0u)  // TODO: is this really possible?
      return {error::incompatible_node_depth};
   if (it->data_type != resp3::type::array) {
      return {error::invalid_data_type};
   }
   const std::size_t num_servers = it->aggregate_size;
   ++it;

   // Each element in the array represents a server
   out.resize(num_servers);
   for (std::size_t i = 0u; i < num_servers; ++i) {
      // A server is a map (resp3) or array (resp2, currently unsupported)
      BOOST_ASSERT(it != last);
      if (it->data_type != resp3::type::map) {
         return {error::invalid_data_type};
      }
      const std::size_t num_key_values = it->aggregate_size;
      ++it;

      // The server object is composed by a set of key/value pairs.
      // Skip everything except for the ones we care for.
      bool ip_seen = false, port_seen = false;
      for (std::size_t j = 0; j < num_key_values; ++j) {
         // Key
         BOOST_ASSERT(it != last);
         if (it->data_type != resp3::type::blob_string)
            return {error::invalid_data_type};
         if (it->depth != 2u)
            return {error::incompatible_node_depth};

         // Value
         if (it->value == "ip") {
            // This is the IP. Skip to the value
            ++it;

            // Parse the value
            BOOST_ASSERT(it != last);
            if (it->data_type != resp3::type::blob_string)
               return {error::invalid_data_type};
            if (it->depth != 2u)
               return {error::incompatible_node_depth};
            ip_seen = true;
            out[i].host = it->value;

            // Move to the next key
            ++it;
         } else if (it->value == "port") {
            // This is the port. Skip to the value
            ++it;

            // Parse it
            BOOST_ASSERT(it != last);
            if (it->data_type != resp3::type::blob_string)
               return {error::invalid_data_type};
            if (it->depth != 2u)
               return {error::incompatible_node_depth};
            port_seen = true;
            out[i].port = it->value;

            // Skip to the next key
            ++it;
         } else {
            // This is an unknown key. Skip the value.
            ++it;

            // TODO: support aggregates
            BOOST_ASSERT(it != last);
            if (it->depth != 2u)
               return {error::incompatible_node_depth};
            if (resp3::is_aggregate(it->data_type))
               return {error::nested_aggregate_not_supported};

            // Skip to the next key
            ++it;
         }
      }

      // Check that the response actually contained the fields we wanted
      if (!ip_seen || !port_seen)
         return {error::invalid_data_type};  // TODO: better diagnostic here
   }

   // Done
   first = it;
   return system::error_code();
}

// Parses an array of nodes into a sentinel_response.
// The request originating this response should be:
//    <user-supplied commands, as per sentinel_config::setup>
//    SENTINEL GET-MASTER-ADDR-BY-NAME
//    SENTINEL REPLICAS (only if server_role is replica)
//    SENTINEL SENTINELS
// expected_responses is required to skip responses to user-supplied setup requests.
// SENTINEL SENTINELS and SENTINEL REPLICAS error when the master name is unknown. Error nodes
// should be allowed in the node array.
// This means that we can't use generic_response, since its adapter errors on error nodes.
// SENTINEL GET-MASTER-ADDR-BY-NAME is sent even when connecting to replicas
//    for better diagnostics when the master name is unknown.
// Preconditions:
//   * expected_responses >= 2 (server_role master) or 3 (server_role replica)
//   * The node array originates from parsing a valid RESP3 message.
//     E.g. we won't check that the first node has depth 0.
inline system::error_code parse_sentinel_response(
   span<const resp3::node> nodes,
   std::size_t expected_responses,
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

   BOOST_ASSERT(expected_responses >= (server_role == role::replica ? 3u : 2u));

   // Clear the output
   out.diagnostic.clear();
   out.sentinels.clear();

   const resp3::node* it = nodes.begin();
   const resp3::node* last = nodes.end();

   // Skip all responses expected the last two ones,
   // because they belong to the user-supplied setup request.
   std::size_t response_idx = 0u;
   for (;; ++it) {
      // The higher levels already ensure that the expected number
      // of root nodes are present, so this is a precondition in this function.
      BOOST_ASSERT(it != last);

      // A root node marks the start of a response.
      // Exit once we're at the start of the first response of interest
      if (it->depth == 0u && ++response_idx == expected_responses - 1u)
         break;

      // Check for errors
      if (auto ec = check_errors(*it))
         return ec;
   }

   // SENTINEL GET-MASTER-ADDR-BY-NAME

   // Check for errors
   if (auto ec = check_errors(*it))
      return ec;

   // If the root node is NULL, Sentinel doesn't know about this master
   if (it->data_type == resp3::type::null) {
      return {error::sentinel_unknown_master};
   }

   // If the root node is an array, an IP and port follow
   if (it->data_type != resp3::type::array)
      return {error::invalid_data_type};
   if (it->aggregate_size != 2u)
      return {error::incompatible_size};
   ++it;

   // IP
   BOOST_ASSERT(it != last);
   if (it->depth != 1u)
      return {error::incompatible_node_depth};
   if (it->data_type != resp3::type::blob_string)
      return {error::invalid_data_type};
   out.master_addr.host = it->value;
   ++it;

   // Port
   BOOST_ASSERT(it != last);
   if (it->depth != 1u)
      return {error::incompatible_node_depth};
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

// An adapter like generic_response, but without checking for error nodes.
// Exposed for testing
inline any_adapter make_vector_adapter(std::vector<resp3::node>& output)
{
   return any_adapter::impl_t(
      [&output](any_adapter::parse_event ev, resp3::node_view const& nd, system::error_code&) {
         if (ev == any_adapter::parse_event::node) {
            output.push_back({nd.data_type, nd.aggregate_size, nd.depth, std::string(nd.value)});
         }
      });
}

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_CONNECTOR_HPP
