/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_ADAPT_HPP
#define BOOST_REDIS_ADAPT_HPP

#include <boost/redis/resp3/node.hpp>
#include <boost/redis/adapter/adapt.hpp>
#include <boost/redis/adapter/detail/response_traits.hpp>
#include <boost/mp11.hpp>
#include <boost/system.hpp>

#include <tuple>
#include <limits>
#include <string_view>
#include <variant>

namespace boost::redis {

/** @brief Tag used to ignore responses.
 *  @ingroup high-level-api
 *
 *  For example
 *
 *  @code
 *  std::tuple<boost::redis::ignore, std::string, boost::redis::ignore> resp;
 *  @endcode
 *
 *  will cause only the second tuple type to be parsed, the others
 *  will be ignored.
 */
using ignore = adapter::detail::ignore;

namespace detail
{

class ignore_adapter {
public:
   void
   operator()(
      std::size_t, resp3::node<std::string_view> const&, system::error_code&) { }

   [[nodiscard]]
   auto get_supported_response_size() const noexcept
      { return static_cast<std::size_t>(-1);}
};

template <class Tuple>
class static_adapter {
private:
   static constexpr auto size = std::tuple_size<Tuple>::value;
   using adapter_tuple = mp11::mp_transform<adapter::adapter_t, Tuple>;
   using variant_type = mp11::mp_rename<adapter_tuple, std::variant>;
   using adapters_array_type = std::array<variant_type, size>;

   adapters_array_type adapters_;

public:
   explicit static_adapter(Tuple& r)
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

} // detail

/** @brief Creates an adapter that ignores responses.
 *  @ingroup high-level-api
 *
 *  This function can be used to create adapters that ignores
 *  responses.
 */
inline auto adapt() noexcept
{
   return detail::response_traits<void>::adapt();
}

/** @brief Adapts a type to be used as a response.
 *  @ingroup high-level-api
 *
 *  The type T must be either
 *
 *  1. a std::tuple<T1, T2, T3, ...> or
 *  2. std::vector<node<String>>
 *
 *  The types T1, T2, etc can be any STL container, any integer type
 *  and `std::string`.
 *
 *  @param t Tuple containing the responses.
 */
template<class T>
auto adapt(T& t) noexcept
{
   return detail::response_traits<T>::adapt(t);
}

} // boost::redis

#endif // BOOST_REDIS_ADAPT_HPP
