/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <aedis/endpoint.hpp>

#include <string>

namespace aedis {

auto is_valid(endpoint const& ep) noexcept -> bool
{
   return !std::empty(ep.host) && !std::empty(ep.port);
}

auto requires_auth(endpoint const& ep) noexcept -> bool
{
   return !std::empty(ep.username) && !std::empty(ep.password);
}

auto operator<<(std::ostream& os, endpoint const& ep) -> std::ostream&
{
   os << ep.host << ":" << ep.port << " (" << ep.username << "," << ep.password << ")";
   return os;
}

} // aedis
