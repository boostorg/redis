/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_ADAPT_HPP
#define AEDIS_ADAPT_HPP

#include <tuple>

#include <boost/mp11.hpp>
#include <boost/variant2.hpp>
#include <boost/utility/string_view.hpp>
#include <boost/system.hpp>

#include <aedis/resp3/node.hpp>
#include <aedis/adapter/adapt.hpp>
#include <aedis/adapter/detail/response_traits.hpp>

namespace aedis {

/** @brief A type that ignores responses.
 *
 *  For example
 *
 *  @code
    std::tuple<aedis::ignore, std::string, aedis::ignore> resp;
 *  @endcode
 *
 *  will cause only the second tuple type to be parsed, the others
 *  will be ignored.
 */
using ignore = adapter::detail::ignore;

namespace detail {

struct ignore_adapter {
   void
   operator()(
      std::size_t i,
      resp3::node<boost::string_view> const& nd,
      boost::system::error_code& ec)
   {
   }

   auto supported_response_size() const noexcept { return std::size_t(-1);}
};

template <class Tuple>
class static_adapter {
private:
   static constexpr auto size = std::tuple_size<Tuple>::value;
   using adapter_tuple = boost::mp11::mp_transform<adapter::adapter_t, Tuple>;
   using variant_type = boost::mp11::mp_rename<adapter_tuple, boost::variant2::variant>;
   using adapters_array_type = std::array<variant_type, size>;

   adapters_array_type adapters_;

public:
   static_adapter(Tuple& r = nullptr)
   {
      adapter::detail::assigner<size - 1>::assign(adapters_, r);
   }

   auto supported_response_size() const noexcept { return size;}

   void
   operator()(
      std::size_t i,
      resp3::node<boost::string_view> const& nd,
      boost::system::error_code& ec)
   {
      using boost::variant2::visit;
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
   vector_adapter(Vector& v) : adapter_{adapter::adapt(v)} { }

   auto supported_response_size() const noexcept { return std::size_t(-1);}

   void
   operator()(
      std::size_t i,
      resp3::node<boost::string_view> const& nd,
      boost::system::error_code& ec)
   {
      adapter_(nd, ec);
   }
};

template <class>
struct response_traits;

template <>
struct response_traits<void> {
   using response_type = void;
   using adapter_type = detail::ignore_adapter;

   static auto adapt() noexcept
      { return detail::ignore_adapter{}; }
};

template <class String, class Allocator>
struct response_traits<std::vector<resp3::node<String>, Allocator>> {
   using response_type = std::vector<resp3::node<String>, Allocator>;
   using adapter_type = vector_adapter<response_type>;

   static auto adapt(response_type& v) noexcept
      { return adapter_type{v}; }
};

template <class ...Ts>
struct response_traits<std::tuple<Ts...>> {
   using response_type = std::tuple<Ts...>;
   using adapter_type = static_adapter<response_type>;

   static auto adapt(response_type& r) noexcept
      { return adapter_type{r}; }
};

} // detail

/** @brief Creates an adapter that ignores responses.
 *
 *  This function can be used to create adapters that ignores
 *  responses. As a result it can improve performance.
 */
auto adapt() noexcept
{
   return detail::response_traits<void>::adapt();
}

/** @brief Adapts a type to be used as a response.
 *
 *  The type T can be any STL container, any integer type and
 *  \c std::string
 */
template<class T>
auto adapt(T& t) noexcept
{
   return detail::response_traits<T>::adapt(t);
}

} // aedis

#endif // AEDIS_ADAPT_HPP
