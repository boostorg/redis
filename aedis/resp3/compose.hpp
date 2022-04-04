/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <string>
#include <string_view>
#include <utility>

namespace aedis {
namespace resp3 {

/** @brief Adds data to stora.
 *  @ingroup any
 */
template <class Storage>
void to_bulk(Storage& to, std::string_view data)
{
   auto const str = std::to_string(std::size(data));

   to += "$";
   to.append(std::cbegin(str), std::cend(str));
   to += "\r\n";
   to += data;
   to += "\r\n";
}

template <class Storage, class T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
void to_bulk(Storage& to, T n)
{
   auto const s = std::to_string(n);
   to_bulk(to, std::string_view{s});
}

namespace detail {

template <class>
struct needs_to_string : std::true_type {};

template <> struct needs_to_string<std::string> : std::false_type {};
template <> struct needs_to_string<std::string_view> : std::false_type {};
template <> struct needs_to_string<char const*> : std::false_type {};
template <> struct needs_to_string<char*> : std::false_type {};

template <std::size_t N>
struct needs_to_string<char[N]> : std::false_type {};

template <std::size_t N>
struct needs_to_string<char const[N]> : std::false_type {};

template <class Storage, class T>
struct add_bulk_impl {
   static void add(Storage& to, T const& from)
   {
      using namespace aedis::resp3;
      to_bulk(to, from);
   }
};

template <class Storage, class U, class V>
struct add_bulk_impl<Storage, std::pair<U, V>> {
   static void add(Storage& to, std::pair<U, V> const& from)
   {
      using namespace aedis::resp3;
      to_bulk(to, from.first);
      to_bulk(to, from.second);
   }
};

} // detail

/** @brief Adds a resp3 header to the store to.
 *  @ingroup any
 */
template <class Storage>
void add_header(Storage& to, std::size_t size)
{
   auto const str = std::to_string(size);

   to += "*";
   to.append(std::cbegin(str), std::cend(str));
   to += "\r\n";
}

/** @brief Adds a rep3 bulk to the storage.
 *  @ingroup any
 *
 *  This function adds \c data as a bulk string to the storage \c to.
 */
template <class Storage, class T>
void add_bulk(Storage& to, T const& data)
{
   detail::add_bulk_impl<Storage, T>::add(to, data);
}

/** @brief Counts the number of bulks required by a given type.
 *  @ingroup any
 */
template <class>
struct bulk_counter;

template <class>
struct bulk_counter {
  static constexpr auto size = 1U;
};

template <class T, class U>
struct bulk_counter<std::pair<T, U>> {
  static constexpr auto size = 2U;
};

} // resp3
} // aedis
