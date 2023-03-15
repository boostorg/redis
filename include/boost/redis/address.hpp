/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_ADDRESS_HPP
#define BOOST_REDIS_ADDRESS_HPP

#include <string>

namespace boost::redis
{

/** @brief Address of a Redis server
 *  @ingroup high-level-api
 */
struct address {
   /// Redis host.
   std::string host = "127.0.0.1";
   /// Redis port.
   std::string port = "6379";
};

} // boost::redis

#endif // BOOST_REDIS_ADDRESS_HPP
