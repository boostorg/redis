/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_RESPONSE_HPP
#define BOOST_REDIS_RESPONSE_HPP

#include <boost/redis/resp3/node.hpp>
#include <boost/redis/adapter/result.hpp>

#include <vector>
#include <string>
#include <tuple>

namespace boost::redis {

/** @brief Response with compile-time size.
 *  @ingroup high-level-api
 */
template <class... Ts>
using response = std::tuple<adapter::result<Ts>...>;

/** @brief A generic response to a request
 *  @ingroup high-level-api
 *
 *  This response type can store any type of RESP3 data structure.  It
 *  contains the
 *  [pre-order](https://en.wikipedia.org/wiki/Tree_traversal#Pre-order,_NLR)
 *  view of the response tree.
 */
using generic_response = adapter::result<std::vector<resp3::node>>;

} // boost::redis::resp3

#endif // BOOST_REDIS_RESPONSE_HPP
