/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <string>
#include <tuple>

#include <boost/hana.hpp>
#include <boost/utility/string_view.hpp>

namespace aedis {
namespace resp3 {

/** @brief Adds data to the request.
 *  @ingroup any
 */
template <class Request>
void to_bulk(Request& to, boost::string_view data)
{
   auto const str = std::to_string(data.size());

   to += "$";
   to.append(std::cbegin(str), std::cend(str));
   to += "\r\n";
   to.append(std::cbegin(data), std::cend(data));
   to += "\r\n";
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

/** @brief Adds a resp3 header to the store to.
 *  @ingroup any
 */
template <class Request>
void add_header(Request& to, std::size_t size)
{
   auto const str = std::to_string(size);

   to += "*";
   to.append(std::cbegin(str), std::cend(str));
   to += "\r\n";
}

/** @brief Adds a rep3 bulk to the request.
 *  @ingroup any
 *
 *  This function adds \c data as a bulk string to the request \c to.
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

} // resp3
} // aedis
