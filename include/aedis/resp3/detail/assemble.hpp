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

#include <aedis/command.hpp>

namespace aedis {
namespace resp3 {
namespace detail {

void add_bulk(std::string& to, std::string_view param);
void add_header(std::string& to, int size);

struct accumulator {
   auto
   operator()(
      std::string a,
      std::string_view b) const
   {
      add_bulk(a, b);
      return a;
   }

   template <class T>
   auto
   operator()(
      std::string a,
      T b,
      typename std::enable_if<(std::is_integral<T>::value || std::is_floating_point<T>::value), bool>::type = false) const
   {
      auto const v = std::to_string(b);
      add_bulk(a, v);
      return a;
   }

   auto
   operator()(
      std::string a,
      std::pair<std::string, std::string_view> b) const
   {
      add_bulk(a, b.first);
      add_bulk(a, b.second);
      return a;
   }

   template <class T>
   auto
   operator()(
      std::string a,
      std::pair<T, std::string_view> b,
      typename std::enable_if<(std::is_integral<T>::value || std::is_floating_point<T>::value), bool>::type = false) const
   {
      auto const v = std::to_string(b.first);
      add_bulk(a, v);
      add_bulk(a, b.second);
      return a;
   }
};

void assemble(std::string& ret, std::string_view cmd);

template <class Iter>
void assemble( std::string& ret
             , std::string_view cmd
             , std::initializer_list<std::string_view> key
             , Iter begin
             , Iter end
             , int size = 1)
{
   auto const d1 =
      std::distance( std::cbegin(key)
                   , std::cend(key));

   auto const d2 = std::distance(begin, end);

   std::string a;
   add_header(a, 1 + d1 + size * d2);
   add_bulk(a, cmd);

   auto b =
      std::accumulate( std::cbegin(key)
                     , std::cend(key)
                     , std::move(a)
                     , accumulator{});

   ret +=
      std::accumulate( begin
                     , end
                     , std::move(b)
                     , accumulator{});
}

void assemble(std::string& ret, std::string_view cmd, std::string_view key);

} // detail
} // resp3
} // aedis
