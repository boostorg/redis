/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_ADAPTER_ADAPT_HPP
#define AEDIS_ADAPTER_ADAPT_HPP

#include <aedis/adapter/detail/response_traits.hpp>

namespace aedis {
namespace adapter {

template <class T>
using adapter_t = typename detail::adapter_t<T>;

/** \internal
    \brief Creates a dummy response adapter.
    \ingroup any
  
    The adapter returned by this function ignores responses. It is
    useful to avoid wasting time with responses which are not needed.

    Example:

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
auto adapt2() noexcept
   { return detail::response_traits<void>::adapt(); }

/** \internal
 *  \brief Adapts user data to read operations.
 *  \ingroup any
 *
 *  STL containers, \c std::tuple and built-in types are supported and
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
auto adapt2(T& t) noexcept
   { return detail::response_traits<T>::adapt(t); }

} // adapter
} // aedis

#endif // AEDIS_ADAPTER_ADAPT_HPP
