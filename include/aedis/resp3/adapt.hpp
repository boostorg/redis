/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/resp3/detail/response_traits.hpp>

/** \file adapt.hpp
 */

namespace aedis {
namespace resp3 {

/** \brief Creates a void adapter
  
    The adapter returned by this function ignores any data and is
    useful to avoid wasting time with responses on which the user is
    insterested in.

    @code
    co_await async_read(socket, buffer, adapt());
    @endcode
 */
inline
auto adapt() noexcept
   { return response_traits<void>::adapt(); }

/** \brief Adapts user data to the resp3 parser.
  
    For the types supported by this function see `response_traits`.
    For example

    @code
    std::unordered_map<std::string, std::string> cont;
    co_await async_read(socket, buffer, adapt(cont));
    @endcode
 */
template<class T>
auto adapt(T& t) noexcept
   { return response_traits<T>::adapt(t); }

} // resp3
} // aedis
