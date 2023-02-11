/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_DETAIL_ADAPT_HPP
#define BOOST_REDIS_DETAIL_ADAPT_HPP

#include <boost/redis/resp3/node.hpp>
#include <boost/redis/response.hpp>
#include <boost/redis/adapter/adapt.hpp>
#include <boost/redis/adapter/detail/response_traits.hpp>
#include <boost/mp11.hpp>
#include <boost/system.hpp>

#include <tuple>
#include <limits>
#include <string_view>
#include <variant>

namespace boost::redis::detail
{

class ignore_adapter {
public:
   void
   operator()(std::size_t, resp3::node<std::string_view> const& nd, system::error_code& ec)
   {
      switch (nd.data_type) {
         case resp3::type::simple_error: ec = redis::error::resp3_simple_error; break;
         case resp3::type::blob_error: ec = redis::error::resp3_blob_error; break;
         case resp3::type::null: ec = redis::error::resp3_null; break;
         default:;
      }
   }

   [[nodiscard]]
   auto get_supported_response_size() const noexcept
      { return static_cast<std::size_t>(-1);}
};

template <class Response>
class static_adapter {
private:
   static constexpr auto size = std::tuple_size<Response>::value;
   using adapter_tuple = mp11::mp_transform<adapter::adapter_t, Response>;
   using variant_type = mp11::mp_rename<adapter_tuple, std::variant>;
   using adapters_array_type = std::array<variant_type, size>;

   adapters_array_type adapters_;

public:
   explicit static_adapter(Response& r)
   {
      adapter::detail::assigner<size - 1>::assign(adapters_, r);
   }

   [[nodiscard]]
   auto get_supported_response_size() const noexcept
      { return size;}

   void
   operator()(
      std::size_t i,
      resp3::node<std::string_view> const& nd,
      system::error_code& ec)
   {
      using std::visit;
      // I am usure whether this should be an error or an assertion.
      BOOST_ASSERT(i < adapters_.size());
      visit([&](auto& arg){arg(nd, ec);}, adapters_.at(i));
   }
};

template <class Vector>
class vector_adapter {
private:
   using adapter_type = typename adapter::detail::response_traits<Vector>::adapter_type;
   adapter_type adapter_;

public:
   explicit vector_adapter(Vector& v)
   : adapter_{adapter::adapt2(v)}
   { }

   [[nodiscard]]
   auto
   get_supported_response_size() const noexcept
      { return static_cast<std::size_t>(-1);}

   void
   operator()(
      std::size_t,
      resp3::node<std::string_view> const& nd,
      system::error_code& ec)
   {
      adapter_(nd, ec);
   }
};

template <class>
struct response_traits;

template <>
struct response_traits<ignore_t> {
   using response_type = ignore_t;
   using adapter_type = detail::ignore_adapter;

   static auto adapt(response_type&) noexcept
      { return detail::ignore_adapter{}; }
};

template <>
struct response_traits<adapter::result<ignore_t>> {
   using response_type = adapter::result<ignore_t>;
   using adapter_type = detail::ignore_adapter;

   static auto adapt(response_type&) noexcept
      { return detail::ignore_adapter{}; }
};

template <class String, class Allocator>
struct response_traits<adapter::result<std::vector<resp3::node<String>, Allocator>>> {
   using response_type = adapter::result<std::vector<resp3::node<String>, Allocator>>;
   using adapter_type = vector_adapter<response_type>;

   static auto adapt(response_type& v) noexcept
      { return adapter_type{v}; }
};

template <class ...Ts>
struct response_traits<response<Ts...>> {
   using response_type = response<Ts...>;
   using adapter_type = static_adapter<response_type>;

   static auto adapt(response_type& r) noexcept
      { return adapter_type{r}; }
};

template <class Adapter>
class wrapper {
public:
   explicit wrapper(Adapter adapter) : adapter_{adapter} {}

   void operator()(resp3::node<std::string_view> const& node, system::error_code& ec)
      { return adapter_(0, node, ec); }

   [[nodiscard]]
   auto get_supported_response_size() const noexcept
      { return adapter_.get_supported_response_size();}

private:
   Adapter adapter_;
};

template <class Adapter>
auto make_adapter_wrapper(Adapter adapter)
{
   return wrapper{adapter};
}

/** @brief Adapts a type to be used as a response.
 *
 *  The type T must be either
 *
 *  1. a response<T1, T2, T3, ...> or
 *  2. std::vector<node<String>>
 *
 *  The types T1, T2, etc can be any STL container, any integer type
 *  and `std::string`.
 *
 *  @param t Tuple containing the responses.
 */
template<class T>
auto boost_redis_adapt(T& t) noexcept
{
   return response_traits<T>::adapt(t);
}

} // boost::redis::detail

#endif // BOOST_REDIS_DETAIL_ADAPT_HPP
