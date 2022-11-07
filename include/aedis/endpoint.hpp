/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_ENDPOINT_HPP
#define AEDIS_ENDPOINT_HPP

#include <string>

namespace aedis {

/** \brief A Redis endpoint.
 *  \ingroup high-level-api
 */
struct endpoint {
   /// Redis server address.
   std::string host;

   /// Redis server port.
   std::string port;
};

} // aedis

#endif // AEDIS_ENDPOINT_HPP
