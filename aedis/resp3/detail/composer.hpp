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

template <class Storage>
void add_header(Storage& to, int size)
{
   // std::string does not support allocators.
   using std::to_string;
   auto const str = to_string(size);

   to += "*";
   to.append(std::cbegin(str), std::cend(str));
   to += "\r\n";
}

template <class Storage>
void add_bulk(Storage& to, std::string_view data)
{
   // std::string does not support allocators.
   using std::to_string;
   auto const str = to_string(std::size(data));

   to += "$";
   to.append(std::cbegin(str), std::cend(str));
   to += "\r\n";
   to += data;
   to += "\r\n";
}

template <class Storage, class T>
void add_bulk(Storage& to, T const& data, typename std::enable_if<needs_to_string<T>::value, bool>::type = false)
{
  using std::to_string;
  auto const s = to_string(data);
  add_bulk(to, s);
}

// Overload for pairs.
// TODO: Overload for tuples.
template <class Storage, class T1, class T2>
void add_bulk(Storage& to, std::pair<T1, T2> const& pair)
{
  add_bulk(to, pair.first);
  add_bulk(to, pair.second);
}

template <class>
struct value_type_size {
  static constexpr auto size = 1U;
};

template <class T, class U>
struct value_type_size<std::pair<T, U>> {
  static constexpr auto size = 2U;
};

} // detail
} // resp3
} // aedis
