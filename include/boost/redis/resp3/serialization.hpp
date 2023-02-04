/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_RESP3_SERIALIZATION_HPP
#define BOOST_REDIS_RESP3_SERIALIZATION_HPP

#include <boost/redis/resp3/type.hpp>

#include <string>
#include <tuple>

// NOTE: Consider detecting tuples in the type in the parameter pack
// to calculate the header size correctly.

namespace boost::redis::resp3 {
constexpr char const* separator = "\r\n";

/** @brief Adds a bulk to the request.
 *  @relates request
 *
 *  This function is useful in serialization of your own data
 *  structures in a request. For example
 *
 *  @code
 *  void boost_redis_to_bulk(std::string& to, mystruct const& obj)
 *  {
 *     auto const str = // Convert obj to a string.
 *     boost_redis_to_bulk(to, str);
 *  }
 *  @endcode
 *
 *  @param to Storage on which data will be copied into.
 *  @param data Data that will be serialized and stored in @c to.
 *
 *  See more in @ref serialization.
 */
template <class Request>
void boost_redis_to_bulk(Request& to, std::string_view data)
{
   auto const str = std::to_string(data.size());

   to += to_code(type::blob_string);
   to.append(std::cbegin(str), std::cend(str));
   to += separator;
   to.append(std::cbegin(data), std::cend(data));
   to += separator;
}

template <class Request, class T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
void boost_redis_to_bulk(Request& to, T n)
{
   auto const s = std::to_string(n);
   boost_redis_to_bulk(to, std::string_view{s});
}

template <class T>
struct add_bulk_impl {
   template <class Request>
   static void add(Request& to, T const& from)
   {
      using namespace boost::redis::resp3;
      boost_redis_to_bulk(to, from);
   }
};

template <class ...Ts>
struct add_bulk_impl<std::tuple<Ts...>> {
   template <class Request>
   static void add(Request& to, std::tuple<Ts...> const& t)
   {
      auto f = [&](auto const&... vs)
      {
         using namespace boost::redis::resp3;
         (boost_redis_to_bulk(to, vs), ...);
      };

      std::apply(f, t);
   }
};

template <class U, class V>
struct add_bulk_impl<std::pair<U, V>> {
   template <class Request>
   static void add(Request& to, std::pair<U, V> const& from)
   {
      using namespace boost::redis::resp3;
      boost_redis_to_bulk(to, from.first);
      boost_redis_to_bulk(to, from.second);
   }
};

template <class Request>
void add_header(Request& to, type t, std::size_t size)
{
   auto const str = std::to_string(size);

   to += to_code(t);
   to.append(std::cbegin(str), std::cend(str));
   to += separator;
}

template <class Request, class T>
void add_bulk(Request& to, T const& data)
{
   add_bulk_impl<T>::add(to, data);
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
void add_blob(Request& to, std::string_view blob)
{
   to.append(std::cbegin(blob), std::cend(blob));
   to += separator;
}

template <class Request>
void add_separator(Request& to)
{
   to += separator;
}
} // boost::redis::resp3

#endif // BOOST_REDIS_RESP3_SERIALIZATION_HPP
