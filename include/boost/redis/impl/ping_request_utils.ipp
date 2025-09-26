/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/config.hpp>
#include <boost/redis/detail/ping_request_utils.hpp>
#include <boost/redis/request.hpp>

namespace boost::redis::detail {

void compose_ping_request(const config& cfg, request& to)
{
   to.clear();
   to.push("PING", cfg.health_check_id);
}

system::error_code check_ping_response(system::error_code io_ec, const generic_response& resp)
{
   if (io_ec)
      return io_ec;

   // TODO: we should probably introduce a better error code/logging
   // for PONG errors. We defer this until
   // https://github.com/boostorg/redis/issues/104
   if (resp.has_error())
      return error::pong_timeout;

   return system::error_code();
}

}  // namespace boost::redis::detail
