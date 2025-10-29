//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_SENTINEL_ADAPTER_HPP
#define BOOST_REDIS_SENTINEL_ADAPTER_HPP

#include <boost/redis/config.hpp>
#include <boost/redis/detail/connection_state.hpp>
#include <boost/redis/detail/coroutine.hpp>
#include <boost/redis/detail/read_buffer.hpp>
#include <boost/redis/error.hpp>
#include <boost/redis/resp3/node.hpp>
#include <boost/redis/resp3/parser.hpp>
#include <boost/redis/resp3/type.hpp>

#include <boost/system/error_code.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace boost::redis::detail {

class sentinel_adapter {
   sentinel_response* resp_;
   std::size_t remaining_responses_;
   std::size_t sentinel_idx_;
   bool ip_seen_{false}, port_seen_{false};
   int resume_point_{0};

   inline system::error_code check_error(const resp3::node_view& node)
   {
      if (node.data_type == resp3::type::simple_error) {
         resp_->diagnostic = node.value;
         return error::resp3_simple_error;
      } else if (node.data_type == resp3::type::blob_error) {
         resp_->diagnostic = node.value;
         return error::resp3_blob_error;
      } else {
         return {};
      }
   }

   system::error_code on_node_impl(const resp3::node_view& node)
   {
      // An error node should always cause an error
      auto ec = check_error(node);
      if (ec)
         return ec;

      switch (resume_point_) {
         BOOST_REDIS_CORO_INITIAL

         resp_->diagnostic.clear();
         resp_->sentinels.clear();

         // Skip the first N responses, because they belong to the user-supplied setup request.
         // on_finish() takes care of updating this counter
         while (remaining_responses_ > 2u)
            BOOST_REDIS_YIELD(resume_point_, 1, {})

         // SENTINEL GET-MASTER-ADDR-BY-NAME

         // NULL: the sentinel doesn't know about this master
         if (node.data_type == resp3::type::null) {
            return {error::sentinel_unknown_master};
         }

         // Array: an IP and port follow
         if (node.data_type != resp3::type::array)
            return {error::invalid_data_type};
         if (node.aggregate_size != 2u)
            return {error::incompatible_size};
         BOOST_REDIS_YIELD(resume_point_, 2, {})

         // IP
         if (node.depth != 1u)
            return {error::incompatible_node_depth};
         if (node.data_type != resp3::type::blob_string)
            return {error::invalid_data_type};
         resp_->server_addr.host = node.value;
         BOOST_REDIS_YIELD(resume_point_, 3, {})

         // Port
         if (node.depth != 1u)
            return {error::incompatible_node_depth};
         if (node.data_type != resp3::type::blob_string)
            return {error::invalid_data_type};
         resp_->server_addr.port = node.value;
         BOOST_REDIS_YIELD(resume_point_, 4, {})

         // SENTINEL SENTINELS
         // If we got here, Sentinel knows about this master.
         // This is either an array response.
         if (node.depth == 0u)
            return {error::incompatible_node_depth};
         else if (node.data_type != resp3::type::array) {
            return {error::invalid_data_type};
         }
         sentinel_idx_ = node.aggregate_size;
         BOOST_REDIS_YIELD(resume_point_, 5, {})

         // Each element represents a sentinel
         for (; sentinel_idx_ < resp_->sentinels.size(); ++sentinel_idx_) {
            // A Sentinel is an array (resp2) or map (resp3)
            if (node.data_type == resp3::type::array) {
               remaining_responses_ = node.aggregate_size;
            } else if (node.data_type == resp3::type::map) {
               remaining_responses_ = node.aggregate_size * 2u;
            } else {
               return {error::invalid_data_type};
            }
            BOOST_REDIS_YIELD(resume_point_, 6, {})

            // Iterate over all key-value pairs
            ip_seen_ = false;
            port_seen_ = false;
            while (node.depth >= 2u) {
               // Key
               if (node.data_type != resp3::type::blob_string)
                  return {error::invalid_data_type};
               if (node.depth != 2u)
                  return {error::incompatible_node_depth};

               if (node.value == "ip") {
                  BOOST_REDIS_YIELD(resume_point_, 7, {})
                  if (node.data_type != resp3::type::blob_string)
                     return {error::invalid_data_type};
                  if (node.depth != 2u)
                     return {error::incompatible_node_depth};
                  ip_seen_ = true;
                  resp_->sentinels[sentinel_idx_].host = node.value;
               } else if (node.value == "port") {
                  BOOST_REDIS_YIELD(resume_point_, 8, {})
                  if (node.data_type != resp3::type::blob_string)
                     return {error::invalid_data_type};
                  if (node.depth != 2u)
                     return {error::incompatible_node_depth};
                  port_seen_ = true;
                  resp_->sentinels[sentinel_idx_].port = node.value;
               } else {
                  BOOST_REDIS_YIELD(resume_point_, 9, {})
               }

               BOOST_REDIS_YIELD(resume_point_, 10, {})  // we're done with the value
            }

            if (!ip_seen_ || !port_seen_)
               return {error::invalid_data_type};  // TODO: better diagnostic here
         }
      }

      // Done
      return system::error_code();
   }

public:
   sentinel_adapter(std::size_t expected_responses, sentinel_response& response)
   : resp_(&response)
   , remaining_responses_(expected_responses)
   { }

   void on_init() { }
   void on_node(const resp3::node_view& node, system::error_code& ec) { ec = on_node_impl(node); }
   void on_finish() { --remaining_responses_; }
};

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_CONNECTOR_HPP
