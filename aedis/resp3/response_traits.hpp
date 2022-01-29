/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/resp3/type.hpp>
#include <aedis/resp3/adapter/detail/adapters.hpp>

#include <set>
#include <unordered_set>
#include <list>
#include <deque>
#include <vector>
#include <charconv>

namespace aedis {
namespace resp3 {

/** \brief Traits class for response objects.
 *  \ingroup classes
 */
template <class T>
struct response_traits
{
   /// The response type.
   using response_type = T;

   /// The adapter type.
   using adapter_type = adapter::detail::simple<response_type>;

   /// Returns an adapter for the reponse r
   static auto adapt(response_type& r) noexcept { return adapter_type{&r}; }
};

template <class T>
struct response_traits<std::optional<T>>
{
   using response_type = std::optional<T>;
   using adapter_type = adapter::detail::simple_optional<typename response_type::value_type>;
   static auto adapt(response_type& i) noexcept { return adapter_type{&i}; }
};

template <class T, class Allocator>
struct response_traits<std::vector<T, Allocator>>
{
   using response_type = std::vector<T, Allocator>;
   using adapter_type = adapter::detail::vector<response_type>;
   static auto adapt(response_type& v) noexcept { return adapter_type{&v}; }
};

template <>
struct response_traits<node>
{
   using response_type = node;
   using adapter_type = adapter::detail::adapter_node<response_type>;
   static auto adapt(response_type& v) noexcept { return adapter_type{&v}; }
};

template <class Allocator>
struct response_traits<std::vector<node, Allocator>>
{
   using response_type = std::vector<node, Allocator>;
   using adapter_type = adapter::detail::general<response_type>;
   static auto adapt(response_type& v) noexcept { return adapter_type{&v}; }
};

template <class T, class Allocator>
struct response_traits<std::list<T, Allocator>>
{
   using response_type = std::list<T, Allocator>;
   using adapter_type = adapter::detail::list<response_type>;
   static auto adapt(response_type& v) noexcept { return adapter_type{&v}; }
};

template <class T, class Allocator>
struct response_traits<std::deque<T, Allocator>>
{
   using response_type = std::deque<T, Allocator>;
   using adapter_type = adapter::detail::list<response_type>;
   static auto adapt(response_type& v) noexcept { return adapter_type{&v}; }
};

template <class Key, class Compare, class Allocator>
struct response_traits<std::set<Key, Compare, Allocator>>
{
   using response_type = std::set<Key, Compare, Allocator>;
   using adapter_type = adapter::detail::set<response_type>;
   static auto adapt(response_type& s) noexcept { return adapter_type{&s}; }
};

template <class Key, class Hash, class KeyEqual, class Allocator>
struct response_traits<std::unordered_set<Key, Hash, KeyEqual, Allocator>>
{
   using response_type = std::unordered_set<Key, Hash, KeyEqual, Allocator>;
   using adapter_type = adapter::detail::set<response_type>;
   static auto adapt(response_type& s) noexcept { return adapter_type{&s}; }
};

template <class Key, class T, class Compare, class Allocator>
struct response_traits<std::map<Key, T, Compare, Allocator>>
{
   using response_type = std::map<Key, T, Compare, Allocator>;
   using adapter_type = adapter::detail::map<response_type>;
   static auto adapt(response_type& s) noexcept { return adapter_type{&s}; }
};

template <class Key, class Hash, class KeyEqual, class Allocator>
struct response_traits<std::unordered_map<Key, Hash, KeyEqual, Allocator>>
{
   using response_type = std::unordered_map<Key, Hash, KeyEqual, Allocator>;
   using adapter_type = adapter::detail::map<response_type>;
   static auto adapt(response_type& s) noexcept { return adapter_type{&s}; }
};

template <>
struct response_traits<void>
{
   using response_type = void;
   using adapter_type = adapter::detail::ignore;
   static auto adapt() noexcept { return adapter_type{}; }
};

} // resp3
} // aedis
