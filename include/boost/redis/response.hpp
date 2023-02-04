/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_RESPONSE_HPP
#define BOOST_REDIS_RESPONSE_HPP

#include <boost/redis/resp3/node.hpp>

#include <vector>
#include <string>
#include <tuple>

namespace boost::redis {

/** @brief The response to a request.
 *  @ingroup high-level-api
 */
template <class... Ts>
using response = std::tuple<Ts...>;

/** @brief A generic response to a request
 *  @ingroup high-level-api
 */
using generic_response = std::vector<resp3::node<std::string>>;

} // boost::redis::resp3

#endif // BOOST_REDIS_RESPONSE_HPP
