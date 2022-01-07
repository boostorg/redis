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

/** \brief Adapts user data to the resp3 parser.
    \ingroup functions
  
    For the types supported by this function see `response_traits`.
    For example

    Example usage:

    @code
    std::unordered_map<std::string, std::string> cont;
    co_await async_read(socket, buffer, adapt(cont));
    @endcode
 */
template<class T>
auto adapt(T& t) noexcept
   { return detail::response_traits<T>::adapt(t); }

} // resp3
} // aedis
