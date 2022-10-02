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

   /// Expected role if any.
   std::string role{};

   /// Username if authentication is required.
   std::string username{};

   /// Password if authentication is required.
   std::string password{};
};

auto is_valid(endpoint const& ep) noexcept -> bool;
auto requires_auth(endpoint const& ep) noexcept -> bool;
auto operator<<(std::ostream& os, endpoint const& ep) -> std::ostream&;

} // aedis

#endif // AEDIS_ENDPOINT_HPP
