/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_ENDPOINT_HPP
#define AEDIS_ENDPOINT_HPP

#include <string>

namespace aedis {

/** @brief A Redis endpoint.
 */
struct endpoint {
   /// Redis server address.
   std::string host;

   /// Redis server port.
   std::string port;

   /// Role master or replica.
   std::string role{"master"};

   /// Username if authentication is required.
   std::string username{};

   /// Password if authentication is required.
   std::string password{};
};

/// TODO
inline
auto is_valid(endpoint const& ep) noexcept
{
   return !std::empty(ep.host) && !std::empty(ep.port);
}

/// TODO move to .ipp
auto operator<<(std::ostream& os, endpoint const& ep) -> std::ostream&
{
   os << ep.host << ":" << ep.port << " (" << ep.username << "," << ep.password << ")";
   return os;
}

} // aedis

#endif // AEDIS_ENDPOINT_HPP
