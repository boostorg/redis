/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_ADAPT_HPP
#define AEDIS_ADAPT_HPP

#include <tuple>
#include <limits>

#include <boost/mp11.hpp>
#include <boost/variant2.hpp>
#include <boost/utility/string_view.hpp>
#include <boost/system.hpp>

#include <aedis/resp3/node.hpp>
#include <aedis/adapter/adapt.hpp>
#include <aedis/adapter/detail/response_traits.hpp>

namespace aedis {

/** @brief Tag used to ignore responses.
 *  @ingroup any
 *
 *  For example
 *
 *  @code
 *  std::tuple<aedis::ignore, std::string, aedis::ignore> resp;
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
   explicit ignore_adapter(std::size_t max_read_size) : max_read_size_{max_read_size} {}

   void
   operator()(
      std::size_t, resp3::node<boost::string_view> const&, boost::system::error_code&) { }

   [[nodiscard]]
   auto get_supported_response_size() const noexcept
      { return static_cast<std::size_t>(-1);}

   [[nodiscard]]
   auto get_max_read_size(std::size_t) const noexcept
      { return max_read_size_;}

private:
   std::size_t max_read_size_;
};

template <class Tuple>
class static_adapter {
private:
   static constexpr auto size = std::tuple_size<Tuple>::value;
   using adapter_tuple = boost::mp11::mp_transform<adapter::adapter_t, Tuple>;
   using variant_type = boost::mp11::mp_rename<adapter_tuple, boost::variant2::variant>;
   using adapters_array_type = std::array<variant_type, size>;

   adapters_array_type adapters_;
   std::size_t max_read_size_;

public:
   explicit static_adapter(Tuple& r, std::size_t max_read_size)
   : max_read_size_{max_read_size}
   {
      adapter::detail::assigner<size - 1>::assign(adapters_, r);
   }

   [[nodiscard]]
   auto get_supported_response_size() const noexcept
      { return size;}

   [[nodiscard]]
   auto get_max_read_size(std::size_t) const noexcept
      { return max_read_size_;}

   void
   operator()(
      std::size_t i,
      resp3::node<boost::string_view> const& nd,
      boost::system::error_code& ec)
   {
      using boost::variant2::visit;
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
   std::size_t max_read_size_;

public:
   explicit vector_adapter(Vector& v, std::size_t max_read_size)
   : adapter_{adapter::adapt2(v)}
   , max_read_size_{max_read_size}
   { }

   [[nodiscard]]
   auto
   get_supported_response_size() const noexcept
      { return static_cast<std::size_t>(-1);}

   [[nodiscard]]
   auto get_max_read_size(std::size_t) const noexcept
      { return max_read_size_;}

   void
   operator()(
      std::size_t,
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

   static auto adapt(std::size_t max_read_size) noexcept
      { return detail::ignore_adapter{max_read_size}; }
};

template <class String, class Allocator>
struct response_traits<std::vector<resp3::node<String>, Allocator>> {
   using response_type = std::vector<resp3::node<String>, Allocator>;
   using adapter_type = vector_adapter<response_type>;

   static auto adapt(response_type& v, std::size_t max_read_size) noexcept
      { return adapter_type{v, max_read_size}; }
};

template <class ...Ts>
struct response_traits<std::tuple<Ts...>> {
   using response_type = std::tuple<Ts...>;
   using adapter_type = static_adapter<response_type>;

   static auto adapt(response_type& r, std::size_t max_read_size) noexcept
      { return adapter_type{r, max_read_size}; }
};

template <class Adapter>
class wrapper {
public:
   explicit wrapper(Adapter adapter) : adapter_{adapter} {}

   void operator()(resp3::node<boost::string_view> const& node, boost::system::error_code& ec)
      { return adapter_(0, node, ec); }

   [[nodiscard]]
   auto get_supported_response_size() const noexcept
      { return adapter_.get_supported_response_size();}

   [[nodiscard]]
   auto get_max_read_size(std::size_t) const noexcept
      { return adapter_.get_max_read_size(0); }

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
 *  @ingroup any
 *
 *  This function can be used to create adapters that ignores
 *  responses.
 *
 *  @param max_read_size Specifies the maximum size of the read
 *  buffer.
 */
inline auto adapt(std::size_t max_read_size = (std::numeric_limits<std::size_t>::max)()) noexcept
{
   return detail::response_traits<void>::adapt(max_read_size);
}

/** @brief Adapts a type to be used as a response.
 *  @ingroup any
 *
 *  The type T can be any STL container, any integer type and
 *  \c std::string
 *
 *  @param t Tuple containing the responses.
 *  @param max_read_size Specifies the maximum size of the read
 *  buffer.
 */
template<class T>
auto adapt(T& t, std::size_t max_read_size = (std::numeric_limits<std::size_t>::max)()) noexcept
{
   return detail::response_traits<T>::adapt(t, max_read_size);
}

} // aedis

#endif // AEDIS_ADAPT_HPP
