/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <vector>
#include <tuple>

#include <boost/mp11.hpp>
#include <boost/variant2.hpp>

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

template <std::size_t N>
struct assigner2 {
  template <class T1, class T2>
  static void assign(T1& dest, T2& from)
  {
     std::get<N>(dest) = internal_adapt(std::get<N>(from));
     assigner2<N - 1>::assign(dest, from);
  }
};

template <>
struct assigner2<0> {
  template <class T1, class T2>
  static void assign(T1& dest, T2& from)
  {
     std::get<0>(dest) = internal_adapt(std::get<0>(from));
  }
};

} // detail

/** @brief Return a specific adapter from the tuple.
 *  
 *  \param t A tuple of response adapters.
 *  \return The adapter that corresponds to type T.
 */
template <class T, class Tuple>
auto& get(Tuple& t)
{
   return std::get<typename response_traits<T>::adapter_type>(t);
}

template <class Tuple>
using adapters_array_t = 
   std::array<
      boost::mp11::mp_unique<
         boost::mp11::mp_rename<
            boost::mp11::mp_transform<
               response_traits_t, Tuple>,
               boost::variant2::variant>>,
      std::tuple_size<Tuple>::value>;

template <class Tuple>
adapters_array_t<Tuple> make_adapters_array(Tuple& t)
{
   adapters_array_t<Tuple> ret;
   detail::assigner<std::tuple_size<Tuple>::value - 1>::assign(ret, t);
   return ret;
}

/** @brief Transaforms a tuple of responses.
 *
 *  @return Transaforms a tuple of responses into a tuple of adapters.
 */
template <class Tuple>
using adapters_tuple_t = 
         boost::mp11::mp_rename<
            boost::mp11::mp_transform<
               response_traits_t, Tuple>,
               std::tuple>;

/** @brief Make a tuple of adapters.
 *  
 *  \param t Tuple of responses.
 *  \return Tuple of adapters.
 */
template <class Tuple>
auto
make_adapters_tuple(Tuple& t)
{
   adapters_tuple_t<Tuple> ret;
   detail::assigner2<std::tuple_size<Tuple>::value - 1>::assign(ret, t);
   return ret;
}

namespace detail {

template <class Tuple>
class static_aggregate_adapter {
private:
   std::size_t i_ = 0;
   std::size_t aggregate_size_ = 0;
   adapters_array_t<Tuple> adapters_;

public:
   static_aggregate_adapter(Tuple* r = nullptr)
   : adapters_(make_adapters_array(*r))
   {}

   void count(resp3::node<boost::string_view> const& nd)
   {
      if (nd.depth == 1) {
         if (is_aggregate(nd.data_type))
            aggregate_size_ = element_multiplicity(nd.data_type) * nd.aggregate_size;
         else
            ++i_;

         return;
      }

      if (--aggregate_size_ == 0)
         ++i_;
   }

   void
   operator()(
      resp3::node<boost::string_view> const& nd,
      boost::system::error_code& ec)
   {
      using boost::variant2::visit;

      if (nd.depth == 0) {
         auto const real_aggr_size = nd.aggregate_size * element_multiplicity(nd.data_type);
         if (real_aggr_size != std::tuple_size<Tuple>::value)
	    ec = error::incompatible_size;

         return;
      }

      visit([&](auto& arg){arg(nd, ec);}, adapters_[i_]);
      count(nd);
   }
};

} // detail

template <class... Ts>
struct response_traits<std::tuple<Ts...>>
{
   using response_type = std::tuple<Ts...>;
   using adapter_type = detail::static_aggregate_adapter<response_type>;
   static auto adapt(response_type& r) noexcept { return adapter_type{&r}; }
};

} // adapter
} // aedis
