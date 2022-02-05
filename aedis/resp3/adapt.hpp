/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/resp3/response_traits.hpp>

namespace aedis {
namespace resp3 {

/** \brief Creates a void response adapter.
    \ingroup any
  
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
   { return response_traits<void>::adapt(); }

/** \brief Adapts user data to read operations.
 *  \ingroup any
 *
 *  For example
 *  The following types are supported.
 *
 *  - Integer data types e.g. `int`, `unsigned`, etc.
 *
 *  - `std::string`
 *
 *  We also support the following C++ containers
 *
 *  - `std::vector<T>`. Can be used with any RESP3 aggregate type.
 *
 *  - `std::deque<T>`. Can be used with any RESP3 aggregate type.
 *
 *  - `std::list<T>`. Can be used with any RESP3 aggregate type.
 *
 *  - `std::set<T>`. Can be used with RESP3 set type.
 *
 *  - `std::unordered_set<T>`. Can be used with RESP3 set type.
 *
 *  - `std::map<T>`. Can be used with RESP3 hash type.
 *
 *  - `std::unordered_map<T>`. Can be used with RESP3 hash type.
 *
 *  All these types can be wrapped in an `std::optional<T>`. This
 *  function also support \c std::tuple to read the response to
 *  tuples. At the moment this feature supports only transactions that
 *  contain simple types or aggregates that don't contain aggregates
 *  themselves (as in most cases).
 *
 *  Example usage:
 *
 *  @code
 *  std::unordered_map<std::string, std::string> cont;
 *  co_await async_read(socket, buffer, adapt(cont));
 *  @endcode
 * 
 *  For a transaction
 *
 *  @code
    sr.push(command::multi);
    sr.push(command::ping, ...);
    sr.push(command::incr, ...);
    sr.push_range(command::rpush, ...);
    sr.push(command::lrange, ...);
    sr.push(command::incr, ...);
    sr.push(command::exec);

    co_await async_write(socket, buffer(request));

    // Reads the response to a transaction
    std::tuple<std::string, int, int, std::vector<std::string>, int> execs;
    co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt(execs));
 *  @endcode
 */
template<class T>
auto adapt(T& t) noexcept
   { return response_traits<T>::adapt(t); }

} // resp3
} // aedis
