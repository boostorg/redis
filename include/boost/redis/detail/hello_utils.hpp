/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_HELLO_UTILS_HPP
#define BOOST_REDIS_HELLO_UTILS_HPP

#include <boost/redis/config.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/response.hpp>

namespace boost::redis::detail {

void setup_hello_request(config& cfg, request& req);  // might move cfg.setup, for efficiency
void clear_response(generic_response& res);
system::error_code check_hello_response(system::error_code io_ec, const generic_response&);

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_RUNNER_HPP
