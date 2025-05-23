/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_ADAPTER_RESPONSE_TRAITS_HPP
#define BOOST_REDIS_ADAPTER_RESPONSE_TRAITS_HPP

#include <boost/redis/adapter/detail/adapters.hpp>
#include <boost/redis/adapter/ignore.hpp>
#include <boost/redis/adapter/result.hpp>
#include <boost/redis/error.hpp>
#include <boost/redis/ignore.hpp>
#include <boost/redis/resp3/type.hpp>

#include <boost/mp11.hpp>

#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

namespace boost::redis::adapter::detail {

/* Traits class for response objects.
 *
 * Provides traits for all supported response types i.e. all STL
 * containers and C++ buil-in types.
 */
template <class Result>
struct result_traits {
   using adapter_type = wrapper<typename std::decay<Result>::type>;
   static auto adapt(Result& r) noexcept { return adapter_type{&r}; }
};

template <>
struct result_traits<result<ignore_t>> {
   using response_type = result<ignore_t>;
   using adapter_type = ignore;
   static auto adapt(response_type) noexcept { return adapter_type{}; }
};

template <>
struct result_traits<ignore_t> {
   using response_type = ignore_t;
   using adapter_type = ignore;
   static auto adapt(response_type) noexcept { return adapter_type{}; }
};

template <class T>
struct result_traits<result<resp3::basic_node<T>>> {
   using response_type = result<resp3::basic_node<T>>;
   using adapter_type = adapter::detail::general_simple<response_type>;
   static auto adapt(response_type& v) noexcept { return adapter_type{&v}; }
};

template <class String, class Allocator>
struct result_traits<result<std::vector<resp3::basic_node<String>, Allocator>>> {
   using response_type = result<std::vector<resp3::basic_node<String>, Allocator>>;
   using adapter_type = adapter::detail::general_aggregate<response_type>;
   static auto adapt(response_type& v) noexcept { return adapter_type{&v}; }
};

template <class T>
using adapter_t = typename result_traits<std::decay_t<T>>::adapter_type;

template <class T>
auto internal_adapt(T& t) noexcept
{
   return result_traits<std::decay_t<T>>::adapt(t);
}

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
      dest[0].template emplace<0>(internal_adapt(std::get<0>(from)));
   }
};

template <class Tuple>
class static_aggregate_adapter;

template <class Tuple>
class static_aggregate_adapter<result<Tuple>> {
private:
   using adapters_array_type = std::array<
      mp11::mp_rename<mp11::mp_transform<adapter_t, Tuple>, std::variant>,
      std::tuple_size<Tuple>::value>;

   // Tuple element we are currently on.
   std::size_t i_ = 0;

   // Nested aggregate element counter.
   std::size_t aggregate_size_ = 0;

   adapters_array_type adapters_;
   result<Tuple>* res_ = nullptr;

public:
   explicit static_aggregate_adapter(result<Tuple>* r = nullptr)
   {
      if (r) {
         res_ = r;
         detail::assigner<std::tuple_size<Tuple>::value - 1>::assign(adapters_, r->value());
      }
   }

   template <class String>
   void count(resp3::basic_node<String> const& elem)
   {
      if (elem.depth == 1 && is_aggregate(elem.data_type)) {
         aggregate_size_ = element_multiplicity(elem.data_type) * elem.aggregate_size;
      }

      if (aggregate_size_ == 0) {
         i_ += 1;
      } else {
         aggregate_size_ -= 1;
      }
   }

   template <class String>
   void operator()(resp3::basic_node<String> const& elem, system::error_code& ec)
   {
      using std::visit;

      if (elem.depth == 0) {
         auto const multiplicity = element_multiplicity(elem.data_type);
         auto const real_aggr_size = elem.aggregate_size * multiplicity;
         if (real_aggr_size != std::tuple_size<Tuple>::value)
            ec = redis::error::incompatible_size;

         return;
      }

      visit(
         [&](auto& arg) {
            arg(elem, ec);
         },
         adapters_[i_]);
      count(elem);
   }
};

template <class... Ts>
struct result_traits<result<std::tuple<Ts...>>> {
   using response_type = result<std::tuple<Ts...>>;
   using adapter_type = static_aggregate_adapter<response_type>;
   static auto adapt(response_type& r) noexcept { return adapter_type{&r}; }
};

}  // namespace boost::redis::adapter::detail

#endif  // BOOST_REDIS_ADAPTER_RESPONSE_TRAITS_HPP
