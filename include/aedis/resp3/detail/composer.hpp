/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <queue>
#include <vector>
#include <string>
#include <numeric>
#include <algorithm>
#include <functional>
#include <type_traits>
#include <string_view>
#include <utility>
#include <iterator>

namespace aedis {
namespace resp3 {
namespace detail {

void add_header(std::string& to, int size);
void add_bulk(std::string& to, std::string_view param);

// Overlaod for integer or floating point data types.
template <class T>
void add_bulk(std::string& to, T data, typename std::enable_if<(std::is_integral<T>::value || std::is_floating_point<T>::value), bool>::type = false)
{
  auto const s = std::to_string(data);
  add_bulk(to, s);
}

// Overload for pairs.
template <class T1, class T2>
void add_bulk(std::string& to, std::pair<T1, T2> const& pair)
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

template <class Iter1, class Iter2>
void assemble( std::string& to
             , std::string_view cmd
             , Iter1 begin1
             , Iter1 end1
             , Iter2 begin2
             , Iter2 end2)
{
   using value_type1 = typename std::iterator_traits<Iter1>::value_type;
   using value_type2 = typename std::iterator_traits<Iter2>::value_type;

   auto const f1 = value_type_size<value_type1>::size;
   auto const f2 = value_type_size<value_type2>::size;

   auto const d1 = std::distance(begin1, end1);
   auto const d2 = std::distance(begin2, end2);

   add_header(to, 1 + f1 * d1 + f2 * d2);
   add_bulk(to, cmd);

   for (; begin1 != end1; ++begin1)
      add_bulk(to, *begin1);

   for (; begin2 != end2; ++begin2)
      add_bulk(to, *begin2);
}

template <class Range>
void assemble(std::string& to, std::string_view cmd, Range const& range)
{
   using std::cbegin;
   using std::cend;
   std::initializer_list<std::string_view> dummy = {};
   detail::assemble(to, cmd, cbegin(range), cend(range), cbegin(dummy), cend(dummy));
}

template <class Range1, class Range2>
void assemble(std::string& to, std::string_view cmd, Range1 const& range1, Range2 const& range2)
{
   using std::cbegin;
   using std::cend;
   detail::assemble(to, cmd, cbegin(range1), cend(range1), cbegin(range2), cend(range2));
}

template <class Tp, class... Us>
constexpr decltype(auto) front(Tp&& t, Us&&...) noexcept
{
   return std::forward<Tp>(t);
}

} // detail
} // resp3
} // aedis
