/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

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
template <class ResponseType>
struct response_traits
{
   /// The response type.
   using response_type = ResponseType;

   /// The adapter type.
   using adapter_type = adapter::detail::wrapper<response_type>;

   /// Returns an adapter for the reponse r
   static auto adapt(response_type& r) noexcept { return adapter_type{&r}; }
};

/// Template typedef for response_traits.
template <class T>
using response_traits_t = typename response_traits<T>::adapter_type;

template <class T>
struct response_traits<resp3::node<T>>
{
   using response_type = resp3::node<T>;
   using adapter_type = adapter::detail::general_simple<response_type>;
   static auto adapt(response_type& v) noexcept { return adapter_type{&v}; }
};

template <class String, class Allocator>
struct response_traits<std::vector<resp3::node<String>, Allocator>>
{
   using response_type = std::vector<resp3::node<String>, Allocator>;
   using adapter_type = adapter::detail::general_aggregate<response_type>;
   static auto adapt(response_type& v) noexcept { return adapter_type{&v}; }
};

template <>
struct response_traits<void>
{
   using response_type = void;
   using adapter_type = resp3::detail::ignore_response;
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
     dest[N] = internal_adapt(std::get<N>(from));
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
class static_aggregate_adapter {
private:
   using foo = boost::mp11::mp_rename<boost::mp11::mp_transform<response_traits_t, Tuple>, std::variant>;
   using variant_type = boost::mp11::mp_unique<foo>;

   std::size_t i_ = 0;
   std::size_t aggregate_size_ = 0;
   std::array<variant_type, std::tuple_size<Tuple>::value> adapters_;

public:
   static_aggregate_adapter(Tuple* r)
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
         auto const real_aggr_size = aggregate_size * element_multiplicity(t);
         if (real_aggr_size != std::tuple_size<Tuple>::value)
	    ec = error::incompatible_size;

         return;
      }

      std::visit([&](auto& arg){arg(t, aggregate_size, depth, data, size, ec);}, adapters_[i_]);
      count(t, aggregate_size, depth);
   }
};

} // detail

template <class... Ts>
struct response_traits<std::tuple<Ts...>>
{
   using response_type = std::tuple<Ts...>;
   using adapter_type = adapter::detail::static_aggregate_adapter<response_type>;
   static auto adapt(response_type& r) noexcept { return adapter_type{&r}; }
};

} // adapter
} // aedis
