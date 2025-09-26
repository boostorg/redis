//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_PING_REQUEST_UTILS_HPP
#define BOOST_PING_REQUEST_UTILS_HPP

#include <boost/redis/config.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/response.hpp>

namespace boost::redis::detail {

// Composes the PING request
void compose_ping_request(const config& cfg, request& to);

// Checks that the response to the ping request was successful
system::error_code check_ping_response(system::error_code io_ec, const generic_response&);

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_RUNNER_HPP
