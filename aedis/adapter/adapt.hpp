/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/adapter/response_traits.hpp>

namespace aedis {
namespace adapter {

/** \brief Creates a dummy response adapter.
    \ingroup any
  
    The adapter returned by this function is dummy which means it
    ignores responses. It is useful to avoid wasting time with
    responses which are not needed. For example

    @code
    // Pushes and writes some commands to the server.
    sr.push(command::hello, 3);
    sr.push(command::ping);
    sr.push(command::quit);
    net::write(socket, net::buffer(request));

    // Ignores all responses except for the response to ping.
    std::string buffer;
    resp3::read(socket, dynamic_buffer(buffer), adapt());     // hello
    resp3::read(socket, dynamic_buffer(buffer), adapt(resp)); // ping
    resp3::read(socket, dynamic_buffer(buffer, adapt()));     // quit
    @endcode
 */
inline
auto adapt() noexcept
   { return response_traits<void>::adapt(); }

/** \brief Adapts user data to read operations.
 *  \ingroup any
 *
 *  All STL containers, \c std::tuple and built-in types are supported and
 *  can be used in conjunction with \c boost::optional<T>.
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
 *  sr.push(command::multi);
 *  sr.push(command::ping, ...);
 *  sr.push(command::incr, ...);
 *  sr.push_range(command::rpush, ...);
 *  sr.push(command::lrange, ...);
 *  sr.push(command::incr, ...);
 *  sr.push(command::exec);
 *
 *  co_await async_write(socket, buffer(request));
 *
 *  // Reads the response to a transaction
 *  std::tuple<std::string, int, int, std::vector<std::string>, int> execs;
 *  co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt(execs));
 *  @endcode
 */
template<class T>
auto adapt(T& t) noexcept
   { return response_traits<T>::adapt(t); }

} // adapter
} // aedis
