/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_RESP3_SERIALIZATION_HPP
#define BOOST_REDIS_RESP3_SERIALIZATION_HPP

#include <boost/redis/resp3/parser.hpp>
#include <boost/redis/resp3/type.hpp>

#include <boost/system/system_error.hpp>
#include <boost/throw_exception.hpp>

#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace boost::redis::detail {

enum class pubsub_change_type
{
   none,
   subscribe,
   unsubscribe,
   psubscribe,
   punsubscribe,
};

struct pubsub_change {
   pubsub_change_type type;
   std::size_t channel_offset;
   std::size_t channel_size;
};

struct command_context_access;

}  // namespace boost::redis::detail

namespace boost::redis {

class command_context {
   detail::pubsub_change_type cmd_change_;
   std::vector<detail::pubsub_change>* changes_;
   std::string* payload_;

   friend struct detail::command_context_access;

public:
   // TODO: hide
   command_context(
      detail::pubsub_change_type t,
      std::vector<detail::pubsub_change>& changes,
      std::string& payload) noexcept
   : cmd_change_(t)
   , changes_(&changes)
   , payload_(&payload)
   { }

   void add_argument(std::string_view value);

   // TODO: hide
   std::string& payload() { return *payload_; }

   // TODO: hide
   void parse_last_argument(std::size_t offset);
};

}  // namespace boost::redis

namespace boost::redis::resp3 {

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
 *  The function must add exactly one bulk string RESP3 node.
 *  If you're using `boost_redis_to_bulk` with a string argument,
 *  you're safe.
 *
 *  @param payload Storage on which data will be copied into.
 *  @param data Data that will be serialized and stored in `payload`.
 */
void boost_redis_to_bulk(std::string& payload, std::string_view data);

template <class T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
void boost_redis_to_bulk(std::string& payload, T n)
{
   auto const s = std::to_string(n);
   boost::redis::resp3::boost_redis_to_bulk(payload, std::string_view{s});
}

void add_header(std::string& payload, type t, std::size_t size);
void add_blob(std::string& payload, std::string_view blob);
void add_separator(std::string& payload);

}  // namespace boost::redis::resp3

namespace boost::redis::detail {

template <class T, class = void>
struct has_to_bulk_v1 : std::false_type { };

template <class T>
struct has_to_bulk_v1<
   T,
   decltype(boost_redis_to_bulk(std::declval<std::string&>(), std::declval<const T&>()))>
: std::true_type { };

template <class T>
void add_scalar_argument(command_context ctx, T const& value)
{
   if constexpr (std::is_convertible_v<T, std::string_view>) {
      ctx.add_argument(value);
   } else if constexpr (std::is_integral_v<T>) {
      ctx.add_argument(std::to_string(value));
   } else if constexpr (detail::has_to_bulk_v1<T>::value) {
      using namespace boost::redis::resp3;
      auto offset = ctx.payload().size();
      boost_redis_to_bulk(ctx.payload(), value);
      ctx.parse_last_argument(offset);
   } else {
      using namespace boost::redis::resp3;
      boost_redis_to_bulk(ctx, value);
   }
}

template <class T>
void add_argument(command_context ctx, T const& data)
{
   detail::add_scalar_argument(ctx, data);
}

template <class... Ts>
void add_argument(command_context ctx, std::tuple<Ts...> const& t)
{
   auto f = [&](auto const&... vs) {
      (detail::add_scalar_argument(ctx, vs), ...);
   };

   std::apply(f, t);
}

template <class U, class V>
void add_argument(command_context ctx, std::pair<U, V> const& from)
{
   detail::add_scalar_argument(ctx, from.first);
   detail::add_scalar_argument(ctx, from.second);
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

template <class... T>
struct bulk_counter<std::tuple<T...>> {
   static constexpr auto size = sizeof...(T);
};

}  // namespace boost::redis::detail

// TODO: this belongs to tests
namespace boost::redis::resp3::detail {

template <class Adapter>
void deserialize(std::string_view const& data, Adapter adapter, system::error_code& ec)
{
   adapter.on_init();

   parser parser;
   while (!parser.done()) {
      auto const res = parser.consume(data, ec);
      if (ec)
         return;

      BOOST_ASSERT(res.has_value());

      adapter.on_node(res.value(), ec);
      if (ec)
         return;
   }

   BOOST_ASSERT(parser.get_consumed() == std::size(data));

   adapter.on_done();
}

template <class Adapter>
void deserialize(std::string_view const& data, Adapter adapter)
{
   system::error_code ec;
   deserialize(data, adapter, ec);

   if (ec)
      BOOST_THROW_EXCEPTION(system::system_error{ec});
}

}  // namespace boost::redis::resp3::detail

#endif  // BOOST_REDIS_RESP3_SERIALIZATION_HPP
