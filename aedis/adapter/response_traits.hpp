/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <set>
#include <array>
#include <unordered_set>
#include <unordered_map>
#include <list>
#include <deque>
#include <vector>
#include <charconv>
#include <tuple>
#include <variant>

#include <boost/mp11.hpp>

#include <aedis/resp3/type.hpp>
#include <aedis/resp3/read.hpp>
#include <aedis/adapter/detail/adapters.hpp>
#include <aedis/adapter/error.hpp>

namespace aedis {
namespace adapter {

/** \brief Traits class for response objects.
 *  \ingroup any
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

/// Template typedef for response_traits.
template <class T>
using response_traits_t = typename response_traits<T>::adapter_type;

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

template <class T>
struct response_traits<node<T>>
{
   using response_type = node<T>;
   using adapter_type = adapter::detail::adapter_node<response_type>;
   static auto adapt(response_type& v) noexcept { return adapter_type{&v}; }
};

template <class String, class Allocator>
struct response_traits<std::vector<node<String>, Allocator>>
{
   using response_type = std::vector<node<String>, Allocator>;
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
   using adapter_type = resp3::ignore_response;
   static auto adapt() noexcept { return adapter_type{}; }
};

namespace detail {

// Duplicated here to avoid circular include dependency.
template<class T>
auto internal_adapt(T& t) noexcept
   { return response_traits<T>::adapt(t); }

template <std::size_t N>
struct assigner {
  template <class T1, class T2>
  static void assign(T1& dest, T2& from)
  {
     dest[N].template emplace<N>(internal_adapt(std::get<N>(from)));
     assigner<N - 1>::assign(dest, from);
  }
};

template <>
struct assigner<0> {
  template <class T1, class T2>
  static void assign(T1& dest, T2& from)
  {
     dest[0] = internal_adapt(std::get<0>(from));
  }
};

template <class Tuple>
class flat_transaction_adapter {
private:
   using variant_type =
      boost::mp11::mp_rename<boost::mp11::mp_transform<response_traits_t, Tuple>, std::variant>;

   std::size_t i_ = 0;
   std::size_t aggregate_size_ = 0;
   std::array<variant_type, std::tuple_size<Tuple>::value> adapters_;

public:
   flat_transaction_adapter(Tuple* r)
      { assigner<std::tuple_size<Tuple>::value - 1>::assign(adapters_, *r); }

   void count(resp3::type t, std::size_t aggregate_size, std::size_t depth)
   {
      if (depth == 1) {
         if (is_aggregate(t))
            aggregate_size_ = element_multiplicity(t) * aggregate_size;
         else
            ++i_;

         return;
      }

      if (--aggregate_size_ == 0)
         ++i_;
   }

   void
   operator()(
      resp3::type t,
      std::size_t aggregate_size,
      std::size_t depth,
      char const* data,
      std::size_t size,
      boost::system::error_code& ec)
   {
      if (depth == 0) {
         if (aggregate_size != std::tuple_size<Tuple>::value)
	    ec = error::incompatible_tuple_size;

         return;
      }

      std::visit([&](auto& arg){arg(t, aggregate_size, depth, data, size, ec);}, adapters_[i_]);
      count(t, aggregate_size, depth);
   }
};

} // detail

// The adapter of responses to transactions, move it to its own header?
template <class... Ts>
struct response_traits<std::tuple<Ts...>>
{
   using response_type = std::tuple<Ts...>;
   using adapter_type = adapter::detail::flat_transaction_adapter<response_type>;
   static auto adapt(response_type& r) noexcept { return adapter_type{&r}; }
};

} // adapter
} // aedis
