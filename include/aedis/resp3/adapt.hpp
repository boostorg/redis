/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/resp3/detail/response_traits.hpp>

namespace aedis {
namespace resp3 {

/** \brief Creates a void response adapter.
    \ingroup functions
  
    The adapter returned by this function ignores responses and is
    useful to avoid wasting time with responses which the user is
    insterested in.

    Example usage:

    @code
    co_await async_read(socket, buffer, adapt());
    @endcode
 */
inline
auto adapt() noexcept
   { return detail::response_traits<void>::adapt(); }

/** \brief Adapts user data to read operations.
 *  \ingroup functions
 *
 *  For example
 *  The following types are supported.
 *
 *  1. Integer data types e.g. `int`, `unsigned`, etc.
 *
 *  1. `std::string`
 *
 *  We also support the following C++ containers
 *
 *  1. `std::vector<T>`. Can be used with any RESP3 aggregate type.
 *
 *  1. `std::deque<T>`. Can be used with any RESP3 aggregate type.
 *
 *  1. `std::list<T>`. Can be used with any RESP3 aggregate type.
 *
 *  1. `std::set<T>`. Can be used with RESP3 set type.
 *
 *  1. `std::unordered_set<T>`. Can be used with RESP3 set type.
 *
 *  1. `std::map<T>`. Can be used with RESP3 hash type.
 *
 *  1. `std::unordered_map<T>`. Can be used with RESP3 hash type.
 *
 *  All these types can be wrapped in an `std::optional<T>`.
 *
 *  Example usage:
 *
 *  @code
 *  std::unordered_map<std::string, std::string> cont;
 *  co_await async_read(socket, buffer, adapt(cont));
 *  @endcode
 */
template<class T>
auto adapt(T& t) noexcept
   { return detail::response_traits<T>::adapt(t); }

} // resp3
} // aedis
