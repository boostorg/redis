//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_CONNECTION_STATE_HPP
#define BOOST_REDIS_CONNECTION_STATE_HPP

#include <boost/redis/config.hpp>
#include <boost/redis/detail/multiplexer.hpp>
#include <boost/redis/logger.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/resp3/node.hpp>
#include <boost/redis/response.hpp>

#include <chrono>
#include <random>
#include <string>
#include <vector>

namespace boost::redis::detail {

struct sentinel_response {
   std::string diagnostic;         // In case the server returned an error
   address master_addr;            // Always populated
   std::vector<address> replicas;  // Populated only when connecting to replicas
   std::vector<address> sentinels;
};

// Contains all the members in connection that don't depend on the Executor.
// Makes implementing sans-io algorithms easier
struct connection_state {
   buffered_logger logger;
   config cfg{};
   multiplexer mpx{};
   std::string setup_diagnostic{};
   request ping_req{};

   // Sentinel stuff
   std::default_random_engine eng{static_cast<std::default_random_engine::result_type>(
      std::chrono::steady_clock::now().time_since_epoch().count())};
   std::vector<address> sentinels{};
   std::vector<resp3::node> sentinel_resp_nodes{};  // for parsing
   sentinel_response sentinel_resp{};
};

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_CONNECTOR_HPP
