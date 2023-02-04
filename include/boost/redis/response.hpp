/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_RESPONSE_HPP
#define BOOST_REDIS_RESPONSE_HPP

#include <boost/redis/resp3/node.hpp>
#include <boost/redis/adapter/adapt.hpp>

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
 *
 *  It contains the
 *  [pre-order](https://en.wikipedia.org/wiki/Tree_traversal#Pre-order,_NLR)
 *  view of the response tree.  Any Redis response can be received in
 *  an array of nodes.
 */
using generic_response = std::vector<resp3::node<std::string>>;

/** @brief Type used to ignore responses.
 *  @ingroup high-level-api
 *
 *  For example
 *
 *  @code
 *  response<boost::redis::ignore_t, std::string, boost::redis::ignore_t> resp;
 *  @endcode
 *
 *  will cause only the second tuple type to be parsed, the others
 *  will be ignored.
 */
using ignore_t = adapter::detail::ignore_t;

/** @brief Global ignore object.
 *  @ingroup high-level-api
 *
 *  Can be used to ignore responses to a request
 *
 *  @code
 *  conn->async_exec(req, ignore, ...);
 *  @endcode
 */
extern ignore_t ignore;

} // boost::redis::resp3

#endif // BOOST_REDIS_RESPONSE_HPP
