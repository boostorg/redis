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

struct sentinel_response {
   std::string diagnostic;
   address server_addr;
   std::vector<address> sentinels;
};

class sentinel_adapter {
   sentinel_response* resp_;
   std::size_t remaining_sentinels_;
   std::size_t remaining_nodes_;
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

public:
   sentinel_adapter(sentinel_response& response, std::size_t setup_size)
   : resp_(&response)
   , remaining_nodes_(setup_size + 1u)
   { }

   enum class result_type
   {
      done,
      needs_more,
   };

   struct result {
      result_type type;
      system::error_code ec{};

      result(result_type t) noexcept
      : type(t)
      { }

      result(system::error_code ec) noexcept
      : type(result_type::done)
      , ec(ec)
      { }
   };

   result on_node(const resp3::node_view& node)
   {
      auto ec = check_error(node);
      if (ec)
         return ec;

      switch (resume_point_) {
         BOOST_REDIS_CORO_INITIAL

         // Skip the first N root nodes
         for (; remaining_nodes_ > 0u; --remaining_nodes_) {
            // Root node
            BOOST_REDIS_YIELD(resume_point_, 1, result_type::needs_more)
            BOOST_ASSERT(node.depth == 0u);

            // Anything below it
            do {
               BOOST_REDIS_YIELD(resume_point_, 2, result_type::needs_more)
            } while (node.depth != 0);
         }

         // SENTINEL GET-MASTER-ADDR-BY-NAME
         BOOST_REDIS_YIELD(resume_point_, 3, result_type::needs_more)

         // NULL: the sentinel doesn't know about this master
         if (node.data_type == resp3::type::null) {
            return {error::sentinel_unknown_master};
         }

         // Array: an IP and port follow
         if (node.data_type != resp3::type::array)
            return {error::invalid_data_type};
         if (node.aggregate_size != 2u)
            return {error::incompatible_size};

         // IP
         BOOST_REDIS_YIELD(resume_point_, 4, result_type::needs_more)
         if (node.depth != 1u)
            return {error::incompatible_node_depth};
         if (node.data_type != resp3::type::blob_string)
            return {error::invalid_data_type};
         resp_->server_addr.host = node.value;

         // Port
         BOOST_REDIS_YIELD(resume_point_, 5, result_type::needs_more)
         if (node.depth != 1u)
            return {error::incompatible_node_depth};
         if (node.data_type != resp3::type::blob_string)
            return {error::invalid_data_type};
         resp_->server_addr.port = node.value;

         // SENTINEL SENTINELS
         // If we got here, Sentinel knows about this master.
         // This is either an array response.
         BOOST_REDIS_YIELD(resume_point_, 6, result_type::needs_more)
         if (node.depth == 0u)
            return {error::incompatible_node_depth};
         else if (node.data_type != resp3::type::array) {
            return {error::invalid_data_type};
         }
         remaining_sentinels_ = node.aggregate_size;

         // Each element represents a sentinel
         for (; remaining_sentinels_ > 0u; --remaining_sentinels_) {
            // A Sentinel is an array (resp2) or map (resp3)
            BOOST_REDIS_YIELD(resume_point_, 7, result_type::needs_more)
            if (node.data_type == resp3::type::array) {
               remaining_nodes_ = node.aggregate_size;
            } else if (node.data_type == resp3::type::map) {
               remaining_nodes_ = node.aggregate_size * 2u;
            } else {
               return {error::invalid_data_type};
            }
            resp_->sentinels.emplace_back();

            // Iterate over all key-value pairs
            for (; remaining_nodes_ > 0u; --remaining_nodes_) {
               // Key
               BOOST_REDIS_YIELD(resume_point_, 8, result_type::needs_more)
               if (node.data_type != resp3::type::blob_string)
                  return {error::invalid_data_type};

               if (node.value == "ip") {
                  BOOST_REDIS_YIELD(resume_point_, 9, result_type::needs_more)
                  if (node.data_type != resp3::type::blob_string)
                     return {error::invalid_data_type};
                  resp_->sentinels.front().host = node.value;
               } else if (node.value == "port") {
                  BOOST_REDIS_YIELD(resume_point_, 10, result_type::needs_more)
                  if (node.data_type != resp3::type::blob_string)
                     return {error::invalid_data_type};
                  resp_->sentinels.front().port = node.value;
               } else {
                  BOOST_REDIS_YIELD(resume_point_, 11, result_type::needs_more)
               }
            }

            // TODO: check that ip and port are present
         }
      }

      // Done
      return system::error_code();
   }
};

class sentinel_thing {
   read_buffer* buffer_;
   sentinel_adapter adapter_;
   resp3::parser parser_;

public:
   sentinel_adapter::result on_read_impl(std::size_t bytes_read)
   {
      buffer_->commit(bytes_read);
      system::error_code ec;

      while (true) {
         auto const res = parser_.consume(buffer_->get_commited(), ec);
         if (ec)
            return ec;

         if (!res)
            return sentinel_adapter::result_type::needs_more;

         auto r = adapter_.on_node(*res);

         if (parser_.done()) {
            buffer_->consume(parser_.get_consumed());
            parser_.reset();
         }

         if (r.type == sentinel_adapter::result_type::done) {
            if (r.ec)
               return r;
            else if (!parser_.done())
               return {error::incompatible_node_depth};
            buffer_->consume(parser_.get_consumed());
            return r;
         }
      }
   }

   sentinel_adapter::result on_read(std::size_t bytes_read) { auto res = on_read_impl(bytes_read); }
};

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_CONNECTOR_HPP
