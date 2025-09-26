/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_SETUP_REQUEST_UTILS_HPP
#define BOOST_REDIS_SETUP_REQUEST_UTILS_HPP

#include <boost/redis/config.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/response.hpp>

namespace boost::redis::detail {

// Modifies config::setup to make a request suitable to be sent
// to the server using async_exec
void compose_setup_request(config& cfg);

// Completely clears a response
void clear_response(generic_response& res);

// Checks that the response to the setup request was successful
system::error_code check_setup_response(system::error_code io_ec, const generic_response&);

// Composes the PING request. TODO: move to another file
void compose_ping_request(const config& cfg, request& to);

// Checks that the response to the ping request was successful
system::error_code check_ping_response(system::error_code io_ec, const generic_response&);

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_RUNNER_HPP
