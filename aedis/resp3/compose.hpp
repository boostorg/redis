/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_RESP3_COMPOSE_HPP
#define AEDIS_RESP3_COMPOSE_HPP

#include <string>
#include <tuple>

#include <boost/hana.hpp>
#include <boost/utility/string_view.hpp>

#include <aedis/resp3/type.hpp>

namespace aedis {
namespace resp3 {

constexpr char separator[] = "\r\n";

/** @brief Adds a bulk to the request.
 *  @ingroup any
 *
 *  This function is useful in serialization of your own data
 *  structures in a request. For example
 *
 *  @code
 *  void to_bulk(std::string& to, mystruct const& obj)
 *  {
 *     auto const str = // Convert obj to a string.
 *     resp3::to_bulk(to, str);
 *  }
 *  @endcode
 *
 *  See more in \ref requests-serialization.
 */
template <class Request>
void to_bulk(Request& to, boost::string_view data)
{
   auto const str = std::to_string(data.size());

   to += to_code(type::blob_string);
   to.append(std::cbegin(str), std::cend(str));
   to += separator;
   to.append(std::cbegin(data), std::cend(data));
   to += separator;
}

template <class Request, class T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
void to_bulk(Request& to, T n)
{
   auto const s = std::to_string(n);
   to_bulk(to, boost::string_view{s});
}

namespace detail {

template <class T>
struct add_bulk_impl {
   template <class Request>
   static void add(Request& to, T const& from)
   {
      using namespace aedis::resp3;
      to_bulk(to, from);
   }
};

template <class U, class V>
struct add_bulk_impl<std::pair<U, V>> {
   template <class Request>
   static void add(Request& to, std::pair<U, V> const& from)
   {
      using namespace aedis::resp3;
      to_bulk(to, from.first);
      to_bulk(to, from.second);
   }
};

template <class ...Ts>
struct add_bulk_impl<boost::hana::tuple<Ts...>> {
   template <class Request>
   static void add(Request& to, boost::hana::tuple<Ts...> const& from)
   {
      using boost::hana::for_each;

      // Fold expressions is C++17 so we use hana.
      //(resp3::add_bulk(*request_, args), ...);

      for_each(from, [&](auto const& e) {
         using namespace aedis::resp3;
         to_bulk(to, e);
      });
   }
};

} // detail

/** @brief Adds a resp3 header to the request.
 *  @ingroup any
 *
 *  See mystruct.hpp for an example.
 */
template <class Request>
void add_header(Request& to, type t, std::size_t size)
{
   auto const str = std::to_string(size);

   to += to_code(t);
   to.append(std::cbegin(str), std::cend(str));
   to += separator;
}

/* Adds a rep3 bulk to the request.
 *
 * This function adds \c data as a bulk string to the request \c to.
 */
template <class Request, class T>
void add_bulk(Request& to, T const& data)
{
   detail::add_bulk_impl<T>::add(to, data);
}

template <class>
struct bulk_counter;

template <class>
struct bulk_counter {
  static constexpr auto size = 1U;
};

template <class T, class U>
struct bulk_counter<std::pair<T, U>> {
  static constexpr auto size = 2U;
};

template <class Request>
void add_blob(Request& to, boost::string_view blob)
{
   to.append(std::cbegin(blob), std::cend(blob));
   to += separator;
}

/** @brief Adds a separator to the request.
 *  @ingroup any
 *
 *  See mystruct.hpp for an example.
 */
template <class Request>
void add_separator(Request& to)
{
   to += separator;
}

} // resp3
} // aedis

#endif // AEDIS_RESP3_COMPOSE_HPP
