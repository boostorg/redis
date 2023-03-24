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
 *  @relates boost::redis::request
 *
 *  This function is useful in serialization of your own data
 *  structures in a request. For example
 *
 *  @code
 *  void boost_redis_to_bulk(std::string& payload, mystruct const& obj)
 *  {
 *     auto const str = // Convert obj to a string.
 *     boost_redis_to_bulk(payload, str);
 *  }
 *  @endcode
 *
 *  @param payload Storage on which data will be copied into.
 *  @param data Data that will be serialized and stored in `payload`.
 *
 *  See more in @ref serialization.
 */
void boost_redis_to_bulk(std::string& payload, std::string_view data);

template <class T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
void boost_redis_to_bulk(std::string& payload, T n)
{
   auto const s = std::to_string(n);
   boost::redis::resp3::boost_redis_to_bulk(payload, std::string_view{s});
}

template <class T>
struct add_bulk_impl {
   static void add(std::string& payload, T const& from)
   {
      using namespace boost::redis::resp3;
      boost_redis_to_bulk(payload, from);
   }
};

template <class ...Ts>
struct add_bulk_impl<std::tuple<Ts...>> {
   static void add(std::string& payload, std::tuple<Ts...> const& t)
   {
      auto f = [&](auto const&... vs)
      {
         using namespace boost::redis::resp3;
         (boost_redis_to_bulk(payload, vs), ...);
      };

      std::apply(f, t);
   }
};

template <class U, class V>
struct add_bulk_impl<std::pair<U, V>> {
   static void add(std::string& payload, std::pair<U, V> const& from)
   {
      using namespace boost::redis::resp3;
      boost_redis_to_bulk(payload, from.first);
      boost_redis_to_bulk(payload, from.second);
   }
};

void add_header(std::string& payload, type t, std::size_t size);

template <class T>
void add_bulk(std::string& payload, T const& data)
{
   add_bulk_impl<T>::add(payload, data);
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

void add_blob(std::string& payload, std::string_view blob);
void add_separator(std::string& payload);

} // boost::redis::resp3

#endif // BOOST_REDIS_RESP3_SERIALIZATION_HPP
