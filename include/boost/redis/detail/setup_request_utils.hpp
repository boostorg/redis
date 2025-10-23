/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_SETUP_REQUEST_UTILS_HPP
#define BOOST_REDIS_SETUP_REQUEST_UTILS_HPP

#include <boost/redis/config.hpp>
#include <boost/redis/detail/connection_state.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/resp3/node.hpp>
#include <boost/redis/response.hpp>

#include <boost/system/error_code.hpp>

#include <cstddef>

namespace boost::redis::detail {

// Modifies config::setup to make a request suitable to be sent
// to the server using async_exec
void compose_setup_request(config& cfg);

inline void compose_sentinel_request(const config& cfg, request& to)
{
   // Copy what the user passed us. This should go first,
   // as it may involve authentication
   to = cfg.sentinel.setup;

   // Add the commands we need
   to.push("SENTINEL", "GET-MASTER-ADDR-BY-NAME", cfg.sentinel.master_name);
   to.push("SENTINEL", "SENTINELS", cfg.sentinel.master_name);

   // Set flags
   to.get_config().cancel_if_unresponded = true;
   to.get_config().cancel_on_connection_lost = true;
}

class sentinel_response_adapter {
   std::size_t remaining_;
   sentinel_response* resp_;

public:
   sentinel_response_adapter(std::size_t user_commands, sentinel_response& resp) noexcept
   : remaining_(user_commands)
   , resp_(&resp)
   { }

   void on_init() { }
   void on_done()
   {
      if (remaining_ != 0u)
         --remaining_;
   }

   void on_node(const resp3::node_view& node, system::error_code& ec)
   {
      // TODO
   }
};

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_RUNNER_HPP
