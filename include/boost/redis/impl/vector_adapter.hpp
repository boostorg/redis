/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_VECTOR_ADAPTER_HPP
#define BOOST_REDIS_VECTOR_ADAPTER_HPP

#include <boost/redis/adapter/any_adapter.hpp>
#include <boost/redis/resp3/node.hpp>

#include <boost/system/error_code.hpp>

#include <vector>

namespace boost::redis::detail {

// An adapter like generic_response, but without checking for error nodes.
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

#endif  // BOOST_REDIS_LOGGER_HPP
