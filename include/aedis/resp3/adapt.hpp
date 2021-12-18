/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/command.hpp>
#include <aedis/resp3/type.hpp>
#include <aedis/resp3/node.hpp>
#include <aedis/resp3/serializer.hpp>
#include <aedis/resp3/detail/adapters.hpp>

#include <set>
#include <unordered_set>
#include <list>
#include <deque>
#include <vector>
#include <charconv>

namespace aedis {
namespace resp3 {

template <class>
struct response_traits;

// Adaptor for some non-aggregate data types.
template <class T>
struct response_traits
{
   using response_type = T;
   using adapter_type = detail::adapter_simple<response_type>;
   static auto adapt(response_type& i) noexcept { return adapter_type{i}; }
};

template <class T, class Allocator>
struct response_traits<std::vector<T, Allocator>>
{
   using response_type = std::vector<T, Allocator>;
   using adapter_type = detail::adapter_vector<response_type>;
   static auto adapt(response_type& v) noexcept { return adapter_type{v}; }
};

template <class Allocator>
struct response_traits<std::vector<node, Allocator>>
{
   using response_type = std::vector<node, Allocator>;
   using adapter_type = detail::adapter_general<response_type>;
   static auto adapt(response_type& v) noexcept { return adapter_type{v}; }
};

template <class T, class Allocator>
struct response_traits<std::list<T, Allocator>>
{
   using response_type = std::list<T, Allocator>;
   using adapter_type = detail::adapter_list<response_type>;
   static auto adapt(response_type& v) noexcept { return adapter_type{v}; }
};

template <class T, class Allocator>
struct response_traits<std::deque<T, Allocator>>
{
   using response_type = std::deque<T, Allocator>;
   using adapter_type = detail::adapter_list<response_type>;
   static auto adapt(response_type& v) noexcept { return adapter_type{v}; }
};

template <class Key, class Compare, class Allocator>
struct response_traits<std::set<Key, Compare, Allocator>>
{
   using response_type = std::set<Key, Compare, Allocator>;
   using adapter_type = detail::adapter_set<response_type>;
   static auto adapt(response_type& s) noexcept { return adapter_type{s}; }
};

template <class Key, class Hash, class KeyEqual, class Allocator>
struct response_traits<std::unordered_set<Key, Hash, KeyEqual, Allocator>>
{
   using response_type = std::unordered_set<Key, Hash, KeyEqual, Allocator>;
   using adapter_type = detail::adapter_set<response_type>;
   static auto adapt(response_type& s) noexcept { return adapter_type{s}; }
};

template <class Key, class T, class Compare, class Allocator>
struct response_traits<std::map<Key, T, Compare, Allocator>>
{
   using response_type = std::map<Key, T, Compare, Allocator>;
   using adapter_type = detail::adapter_map<response_type>;
   static auto adapt(response_type& s) noexcept { return adapter_type{s}; }
};

template <>
struct response_traits<void>
{
   using response_type = void;
   using adapter_type = detail::adapter_ignore;
   static auto adapt() noexcept { return adapter_type{}; }
};

/** \brief Creates a void adapter
  
    The adapter returned by this function ignores any data and is
    useful to avoid wasting time with responses on which the user is
    insterested in.
 */
inline
response_traits<void>::adapter_type
adapt() noexcept
{
  return response_traits<void>::adapt();
}

/** \brief Adapts user data to the resp3 parser.
  
    The supported types are.
  
    1. Integers: int, unsigned etc.
    2. std::string
    3. STL containers.
 */
template<class T>
response_traits<T>::adapter_type
adapt(T& t) noexcept
{
  return response_traits<T>::adapt(t);
}

} // resp3
} // aedis
