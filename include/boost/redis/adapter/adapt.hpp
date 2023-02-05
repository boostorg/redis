/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_ADAPTER_ADAPT_HPP
#define BOOST_REDIS_ADAPTER_ADAPT_HPP

#include <boost/redis/adapter/detail/response_traits.hpp>

namespace boost::redis::adapter
{

template <class T>
using adapter_t = typename detail::adapter_t<T>;

/** @brief Adapts user data to read operations.
 *  @ingroup low-level-api
 *
 *  STL containers, \c resp3::response and built-in types are supported and
 *  can be used in conjunction with \c std::optional<T>.
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
 *  resp3::response<std::string, int, int, std::vector<std::string>, int> execs;
 *  co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt(execs));
 *  @endcode
 */
template<class T>
auto adapt2(T& t = redis::ignore) noexcept
   { return detail::response_traits<T>::adapt(t); }

} // boost::redis::adapter

#endif // BOOST_REDIS_ADAPTER_ADAPT_HPP
