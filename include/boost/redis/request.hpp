/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_REQUEST_HPP
#define BOOST_REDIS_REQUEST_HPP

#include <boost/redis/resp3/serialization.hpp>
#include <boost/redis/resp3/type.hpp>

#include <chrono>
#include <initializer_list>
#include <iterator>
#include <string>
#include <string_view>
#include <tuple>

// NOTE: For some commands like hset it would be a good idea to assert
// the value type is a pair.

namespace boost::redis {

namespace detail {
auto has_response(std::string_view cmd) -> bool;
struct request_access;
}  // namespace detail

class set_condition {
public:
   set_condition();  // no condition
   static set_condition nx();
   static set_condition xx();
   static set_condition ifeq(std::string_view value);
   static set_condition ifne(std::string_view value);
   static set_condition ifdeq(std::string_view digest);
   static set_condition ifdne(std::string_view digest);

private:
   union {
      std::chrono::milliseconds ms;
      std::chrono::seconds s;
      std::string_view sv;
   } data_;
};

class set_expiry {
public:
   set_expiry();  // no expiry
   static set_expiry ex(std::chrono::seconds duration);
   static set_expiry px(std::chrono::milliseconds duration);
   static set_expiry exat(std::chrono::system_clock::time_point tp);
   static set_expiry pxat(std::chrono::system_clock::time_point tp);
   static set_expiry keepttl();
};

struct set_args {
   set_condition condition{};
   bool get{false};
   set_expiry expiry{};
};

class getex_args {
public:
   getex_args();
   static getex_args ex(std::chrono::seconds duration);
   static getex_args px(std::chrono::milliseconds duration);
   static getex_args exat(std::chrono::system_clock::time_point tp);
   static getex_args pxat(std::chrono::system_clock::time_point tp);
   static getex_args persist();
};

/** @brief Represents a Redis request.
 *  
 *  A request is composed of one or more Redis commands and is
 *  referred to in the redis documentation as a pipeline. See
 *  <a href="https://redis.io/topics/pipelining"></a> for more info.
 *
 *  For example:
 *
 *  @code
 *  request r;
 *  r.push("SET", "k1", "some_value");
 *  r.push("SET", "k2", "other_value");
 *  r.push("GET", "k3");
 *  @endcode
 *
 *  Uses a `std::string` for internal storage.
 */
class request {
public:
   /// Request configuration options.
   struct config {
      /** @brief (Deprecated) If `true`, calls to @ref basic_connection::async_exec will
       * complete with error if the connection is lost while the
       * request hasn't been sent yet.
       *
       * @par Deprecated
       * This setting is deprecated and should be always left out as the default
       * (waiting for a connection to be established again).
       * If you need to limit how much time a @ref basic_connection::async_exec
       * operation is allowed to take, use `asio::cancel_after`, instead.
       */
      bool cancel_on_connection_lost = false;

      /** @brief (Deprecated) If `true`, @ref basic_connection::async_exec will complete with
       * @ref boost::redis::error::not_connected if the call happens
       * before the connection with Redis was established.
       *
       * @par Deprecated
       * This setting is deprecated and should be always left out as the default
       * (waiting for a connection to be established).
       * If you need to limit how much time a @ref basic_connection::async_exec
       * operation is allowed to take, use `asio::cancel_after`, instead.
       */
      bool cancel_if_not_connected = false;

      /** @brief If `false`, @ref basic_connection::async_exec will not
       * automatically cancel this request if the connection is lost.
       * Affects only requests that have been written to the server
       * but have not been responded when
       * the connection is lost.
       */
      bool cancel_if_unresponded = true;

      /** @brief (Deprecated) If this request has a `HELLO` command and this flag
       * is `true`, it will be moved to the
       * front of the queue of awaiting requests. This makes it
       * possible to send `HELLO` commands and authenticate before other
       * commands are sent.
       *
       * @par Deprecated
       * This field has been superseded by @ref config::setup.
       * This setup request will always be run first on connection establishment.
       * Please use it to run any required setup commands.
       * This field will be removed in subsequent releases.
       */
      bool hello_with_priority = true;
   };

   /** @brief Constructor
    *  
    *  @param cfg Configuration options.
    */
   explicit request(config cfg = config{false, false, true, true})
   : cfg_{cfg}
   { }

   /// Returns the number of responses expected for this request.
   [[nodiscard]] auto get_expected_responses() const noexcept -> std::size_t
   {
      return expected_responses_;
   };

   /// Returns the number of commands contained in this request.
   [[nodiscard]] auto get_commands() const noexcept -> std::size_t { return commands_; };

   [[nodiscard]] auto payload() const noexcept -> std::string_view { return payload_; }

   [[nodiscard]]
   BOOST_DEPRECATED(
      "The hello_with_priority attribute and related functions are deprecated. "
      "Use config::setup to run setup commands, instead.") auto has_hello_priority() const noexcept
      -> auto const&
   {
      return has_hello_priority_;
   }

   /// Clears the request preserving allocated memory.
   void clear()
   {
      payload_.clear();
      commands_ = 0;
      expected_responses_ = 0;
      has_hello_priority_ = false;
   }

   /// Calls `std::string::reserve` on the internal storage.
   void reserve(std::size_t new_cap = 0) { payload_.reserve(new_cap); }

   /// Returns a reference to the config object.
   [[nodiscard]] auto get_config() const noexcept -> config const& { return cfg_; }

   /// Returns a reference to the config object.
   [[nodiscard]] auto get_config() noexcept -> config& { return cfg_; }

   /** @brief Appends a new command to the end of the request.
    *
    *  For example:
    *
    *  @code
    *  request req;
    *  req.push("SET", "key", "some string", "EX", 2);
    *  @endcode
    *
    *  This will add a `SET` command with value `"some string"` and an
    *  expiration of 2 seconds.
    *
    *  Command arguments should either be convertible to `std::string_view`,
    *  integral types, or support the `boost_redis_to_bulk` function.
    *  This function is a customization point that must be made available
    *  using ADL and must have the following signature:
    *
    *  @code
    *  void boost_redis_to_bulk(std::string& to, T const& t);
    *  @endcode
    *
    *
    *  See cpp20_serialization.cpp
    *
    *  @param cmd The command to execute. It should be a redis or sentinel command, like `"SET"`.
    *  @param args Command arguments. `args` is allowed to be empty.
    *  @tparam Ts Types of the command arguments.
    *
    */
   template <class... Ts>
   void push(std::string_view cmd, Ts const&... args)
   {
      auto constexpr pack_size = sizeof...(Ts);
      resp3::add_header(payload_, resp3::type::array, 1 + pack_size);
      resp3::add_bulk(payload_, cmd);
      resp3::add_bulk(payload_, std::tie(std::forward<Ts const&>(args)...));

      check_cmd(cmd);
   }

   /** @brief Appends a new command to the end of the request.
    *  
    *  This overload is useful for commands that have a key and have a
    *  dynamic range of arguments. For example:
    *
    *  @code
    *  std::map<std::string, std::string> map
    *     { {"key1", "value1"}
    *     , {"key2", "value2"}
    *     , {"key3", "value3"}
    *     };
    *
    *  request req;
    *  req.push_range("HSET", "key", map.cbegin(), map.cend());
    *  @endcode
    *
    *  This will generate the following command:
    *
    *  @code
    *  HSET key key1 value1 key2 value2 key3 value3
    *  @endcode
    *
    *  *If the passed range is empty, no command is added* and this
    *  function becomes a no-op.
    *
    *  The value type of the passed range should satisfy one of the following:
    *
    *    @li The type is convertible to `std::string_view`. One argument is added
    *        per element in the range.
    *    @li The type is an integral type. One argument is added
    *        per element in the range.
    *    @li The type supports the `boost_redis_to_bulk` function. One argument is added
    *        per element in the range. This function is a customization point that must be made available
    *        using ADL and must have the signature `void boost_redis_to_bulk(std::string& to, T const& t);`.
    *    @li The type is a `std::pair` instantiation, with both arguments supporting one of
    *        the points above. Two arguments are added per element in the range.
    *        Nested pairs are not allowed.
    *    @li The type is a `std::tuple` instantiation, with every argument supporting
    *        one of the points above. N arguments are added per element in the range,
    *        with N being the tuple size. Nested tuples are not allowed. 
    *        
    *  @param cmd The command to execute. It should be a redis or sentinel command, like `"SET"`.
    *  @param key The command key. It will be added as the first argument to the command.
    *  @param begin Iterator to the begin of the range.
    *  @param end Iterator to the end of the range.
    *  @tparam ForwardIterator A forward iterator with an element type that supports one of the points above.
    *
    *  See cpp20_serialization.cpp
    */
   template <class ForwardIterator>
   void push_range(
      std::string_view cmd,
      std::string_view key,
      ForwardIterator begin,
      ForwardIterator end,
      typename std::iterator_traits<ForwardIterator>::value_type* = nullptr)
   {
      using value_type = typename std::iterator_traits<ForwardIterator>::value_type;

      if (begin == end)
         return;

      auto constexpr size = resp3::bulk_counter<value_type>::size;
      auto const distance = std::distance(begin, end);
      resp3::add_header(payload_, resp3::type::array, 2 + size * distance);
      resp3::add_bulk(payload_, cmd);
      resp3::add_bulk(payload_, key);

      for (; begin != end; ++begin)
         resp3::add_bulk(payload_, *begin);

      check_cmd(cmd);
   }

   /** @brief Appends a new command to the end of the request.
    *
    *  This overload is useful for commands that have a dynamic number
    *  of arguments and don't have a key. For example:
    *
    *  @code
    *  std::set<std::string> channels
    *     { "channel1" , "channel2" , "channel3" };
    *
    *  request req;
    *  req.push("SUBSCRIBE", channels.cbegin(), channels.cend());
    *  @endcode
    *
    *  This will generate the following command:
    *
    *  @code
    *  SUBSCRIBE channel1 channel2 channel3
    *  @endcode
    *
    *  *If the passed range is empty, no command is added* and this
    *  function becomes a no-op.
    *
    *  The value type of the passed range should satisfy one of the following:
    *
    *    @li The type is convertible to `std::string_view`. One argument is added
    *        per element in the range.
    *    @li The type is an integral type. One argument is added
    *        per element in the range.
    *    @li The type supports the `boost_redis_to_bulk` function. One argument is added
    *        per element in the range. This function is a customization point that must be made available
    *        using ADL and must have the signature `void boost_redis_to_bulk(std::string& to, T const& t);`.
    *    @li The type is a `std::pair` instantiation, with both arguments supporting one of
    *        the points above. Two arguments are added per element in the range.
    *        Nested pairs are not allowed.
    *    @li The type is a `std::tuple` instantiation, with every argument supporting
    *        one of the points above. N arguments are added per element in the range,
    *        with N being the tuple size. Nested tuples are not allowed.
    *  
    *  @param cmd The command to execute. It should be a redis or sentinel command, like `"SET"`.
    *  @param begin Iterator to the begin of the range.
    *  @param end Iterator to the end of the range.
    *  @tparam ForwardIterator A forward iterator with an element type that supports one of the points above.
    *
    *  See cpp20_serialization.cpp
    */
   template <class ForwardIterator>
   void push_range(
      std::string_view cmd,
      ForwardIterator begin,
      ForwardIterator end,
      typename std::iterator_traits<ForwardIterator>::value_type* = nullptr)
   {
      using value_type = typename std::iterator_traits<ForwardIterator>::value_type;

      if (begin == end)
         return;

      auto constexpr size = resp3::bulk_counter<value_type>::size;
      auto const distance = std::distance(begin, end);
      resp3::add_header(payload_, resp3::type::array, 1 + size * distance);
      resp3::add_bulk(payload_, cmd);

      for (; begin != end; ++begin)
         resp3::add_bulk(payload_, *begin);

      check_cmd(cmd);
   }

   /** @brief Appends a new command to the end of the request.
    *  
    *  Equivalent to the overload taking a range of begin and end
    *  iterators.
    *
    *  *If the passed range is empty, no command is added* and this
    *  function becomes a no-op.
    *
    *  The value type of the passed range should satisfy one of the following:
    *
    *    @li The type is convertible to `std::string_view`. One argument is added
    *        per element in the range.
    *    @li The type is an integral type. One argument is added
    *        per element in the range.
    *    @li The type supports the `boost_redis_to_bulk` function. One argument is added
    *        per element in the range. This function is a customization point that must be made available
    *        using ADL and must have the signature `void boost_redis_to_bulk(std::string& to, T const& t);`.
    *    @li The type is a `std::pair` instantiation, with both arguments supporting one of
    *        the points above. Two arguments are added per element in the range.
    *        Nested pairs are not allowed.
    *    @li The type is a `std::tuple` instantiation, with every argument supporting
    *        one of the points above. N arguments are added per element in the range,
    *        with N being the tuple size. Nested tuples are not allowed.
    *  
    *  @param cmd The command to execute. It should be a redis or sentinel command, like `"SET"`.
    *  @param key The command key. It will be added as the first argument to the command.
    *  @param range Range containing the command arguments.
    *  @tparam Range A type that can be passed to `std::begin()` and `std::end()` to obtain
    *          iterators.
    */
   template <class Range>
   void push_range(
      std::string_view cmd,
      std::string_view key,
      Range const& range,
      decltype(std::begin(range))* = nullptr)
   {
      using std::begin;
      using std::end;
      push_range(cmd, key, begin(range), end(range));
   }

   /** @brief Appends a new command to the end of the request.
    *
    *  Equivalent to the overload taking a range of begin and end
    *  iterators.
    *
    *  *If the passed range is empty, no command is added* and this
    *  function becomes a no-op.
    *
    *  The value type of the passed range should satisfy one of the following:
    *
    *    @li The type is convertible to `std::string_view`. One argument is added
    *        per element in the range.
    *    @li The type is an integral type. One argument is added
    *        per element in the range.
    *    @li The type supports the `boost_redis_to_bulk` function. One argument is added
    *        per element in the range. This function is a customization point that must be made available
    *        using ADL and must have the signature `void boost_redis_to_bulk(std::string& to, T const& t);`.
    *    @li The type is a `std::pair` instantiation, with both arguments supporting one of
    *        the points above. Two arguments are added per element in the range.
    *        Nested pairs are not allowed.
    *    @li The type is a `std::tuple` instantiation, with every argument supporting
    *        one of the points above. N arguments are added per element in the range,
    *        with N being the tuple size. Nested tuples are not allowed.
    *  
    *  @param cmd The command to execute. It should be a redis or sentinel command, like `"SET"`.
    *  @param range Range containing the command arguments.
    *  @tparam Range A type that can be passed to `std::begin()` and `std::end()` to obtain
    *          iterators.
    */
   template <class Range>
   void push_range(
      std::string_view cmd,
      Range const& range,
      decltype(std::cbegin(range))* = nullptr)
   {
      using std::cbegin;
      using std::cend;
      push_range(cmd, cbegin(range), cend(range));
   }

   /** @brief Appends the commands in another request to the end of the request.
    *
    *  Appends all the commands contained in `other` to the end of
    *  this request. Configuration flags in `*this`,
    *  like @ref config::cancel_if_unresponded, are *not* modified,
    *  even if `other` has a different config than `*this`.
    *  
    *  @param other The request containing the commands to append.
    */
   void append(const request& other);

   /**
    * @brief Appends a SUBSCRIBE command to the end of the request.
    *
    * If `channels` contains `{"ch1", "ch2"}`, the resulting command
    * is `SUBSCRIBE ch1 ch2`.
    *
    * SUBSCRIBE commands created using this function are tracked
    * to enable PubSub state restoration. After successfully executing
    * the request, the list of subscribed channels is stored
    * in the connection. Every time a reconnection happens,
    * a suitable `SUBSCRIBE` command is issued automatically,
    * to restore the subscriptions that were active before the reconnection.
    * 
    * PubSub store restoration only happens when using `push_subscribe`.
    * Use @ref push or @ref push_range to disable it.
    */
   void push_subscribe(std::initializer_list<std::string_view> channels)
   {
      push_subscribe(channels.begin(), channels.end());
   }

   /**
    * @brief Appends a SUBSCRIBE command to the end of the request.
    *
    * If `channels` contains `["ch1", "ch2"]`, the resulting command
    * is `SUBSCRIBE ch1 ch2`.
    *
    * SUBSCRIBE commands created using this function are tracked
    * to enable PubSub state restoration. After successfully executing
    * the request, the list of subscribed channels is stored
    * in the connection. Every time a reconnection happens,
    * a suitable `SUBSCRIBE` command is issued automatically,
    * to restore the subscriptions that were active before the reconnection.
    * 
    * PubSub store restoration only happens when using @ref push_subscribe,
    * @ref push_unsubscribe, @ref push_psubscribe and @ref push_punsubscribe.
    * Use @ref push or @ref push_range to disable it.
    */
   template <class Range>
   void push_subscribe(Range&& channels, decltype(std::cbegin(channels))* = nullptr)
   {
      push_subscribe(std::cbegin(channels), std::cend(channels));
   }

   /**
    * @brief Appends a SUBSCRIBE command to the end of the request.
    *
    * [`channels_begin`, `channels_end`) should point to a valid
    * range of elements convertible to `std::string_view`.
    * If the range contains `["ch1", "ch2"]`, the resulting command
    * is `SUBSCRIBE ch1 ch2`.
    *
    * SUBSCRIBE commands created using this function are tracked
    * to enable PubSub state restoration. After successfully executing
    * the request, the list of subscribed channels is stored
    * in the connection. Every time a reconnection happens,
    * a suitable `SUBSCRIBE` command is issued automatically,
    * to restore the subscriptions that were active before the reconnection.
    * 
    * PubSub store restoration only happens when using @ref push_subscribe,
    * @ref push_unsubscribe, @ref push_psubscribe and @ref push_punsubscribe.
    * Use @ref push or @ref push_range to disable it.
    */
   template <class ForwardIt>
   void push_subscribe(ForwardIt channels_begin, ForwardIt channels_end)
   {
      push_range("SUBSCRIBE", channels_begin, channels_end);
   }

   /**
    * @brief Appends an UNSUBSCRIBE command to the end of the request.
    *
    * If `channels` contains `{"ch1", "ch2"}`, the resulting command
    * is `UNSUBSCRIBE ch1 ch2`.
    *
    * UNSUBSCRIBE commands created using this function are tracked
    * to enable PubSub state restoration. After successfully executing
    * the request, the list of subscribed channels tracked by the
    * connection is updated.
    * 
    * PubSub store restoration only happens when using @ref push_subscribe,
    * @ref push_unsubscribe, @ref push_psubscribe and @ref push_punsubscribe.
    * Use @ref push or @ref push_range to disable it.
    */
   void push_unsubscribe(std::initializer_list<std::string_view> channels)
   {
      push_unsubscribe(channels.begin(), channels.end());
   }

   /**
    * @brief Appends an UNSUBSCRIBE command to the end of the request.
    *
    * If `channels` contains `["ch1", "ch2"]`, the resulting command
    * is `UNSUBSCRIBE ch1 ch2`.
    *
    * UNSUBSCRIBE commands created using this function are tracked
    * to enable PubSub state restoration. After successfully executing
    * the request, the list of subscribed channels tracked by the
    * connection is updated.
    * 
    * PubSub store restoration only happens when using @ref push_subscribe,
    * @ref push_unsubscribe, @ref push_psubscribe and @ref push_punsubscribe.
    * Use @ref push or @ref push_range to disable it.
    */
   template <class Range>
   void push_unsubscribe(Range&& channels, decltype(std::cbegin(channels))* = nullptr)
   {
      push_unsubscribe(std::cbegin(channels), std::cend(channels));
   }

   /**
    * @brief Appends an UNSUBSCRIBE command to the end of the request.
    *
    * [`channels_begin`, `channels_end`) should point to a valid
    * range of elements convertible to `std::string_view`.
    * If the range contains `["ch1", "ch2"]`, the resulting command
    * is `UNSUBSCRIBE ch1 ch2`.
    *
    * UNSUBSCRIBE commands created using this function are tracked
    * to enable PubSub state restoration. After successfully executing
    * the request, the list of subscribed channels tracked by the
    * connection is updated.
    * 
    * PubSub store restoration only happens when using @ref push_subscribe,
    * @ref push_unsubscribe, @ref push_psubscribe and @ref push_punsubscribe.
    * Use @ref push or @ref push_range to disable it.
    */
   template <class ForwardIt>
   void push_unsubscribe(ForwardIt channels_begin, ForwardIt channels_end)
   {
      push_range("UNSUBSCRIBE", channels_begin, channels_end);
   }

   /**
    * @brief Appends a PSUBSCRIBE command to the end of the request.
    *
    * If `patterns` contains `{"news.*", "events.*"}`, the resulting command
    * is `PSUBSCRIBE news.* events.*`.
    *
    * PSUBSCRIBE commands created using this function are tracked
    * to enable PubSub state restoration. After successfully executing
    * the request, the list of subscribed patterns is stored
    * in the connection. Every time a reconnection happens,
    * a suitable `PSUBSCRIBE` command is issued automatically,
    * to restore the subscriptions that were active before the reconnection.
    * 
    * PubSub store restoration only happens when using @ref push_subscribe,
    * @ref push_unsubscribe, @ref push_psubscribe and @ref push_punsubscribe.
    * Use @ref push or @ref push_range to disable it.
    */
   void push_psubscribe(std::initializer_list<std::string_view> patterns)
   {
      push_psubscribe(patterns.begin(), patterns.end());
   }

   /**
    * @brief Appends a PSUBSCRIBE command to the end of the request.
    *
    * If `patterns` contains `["news.*", "events.*"]`, the resulting command
    * is `PSUBSCRIBE news.* events.*`.
    *
    * PSUBSCRIBE commands created using this function are tracked
    * to enable PubSub state restoration. After successfully executing
    * the request, the list of subscribed patterns is stored
    * in the connection. Every time a reconnection happens,
    * a suitable `PSUBSCRIBE` command is issued automatically,
    * to restore the subscriptions that were active before the reconnection.
    * 
    * PubSub store restoration only happens when using @ref push_subscribe,
    * @ref push_unsubscribe, @ref push_psubscribe and @ref push_punsubscribe.
    * Use @ref push or @ref push_range to disable it.
    */
   template <class Range>
   void push_psubscribe(Range&& patterns, decltype(std::cbegin(patterns))* = nullptr)
   {
      push_psubscribe(std::cbegin(patterns), std::cend(patterns));
   }

   /**
    * @brief Appends a PSUBSCRIBE command to the end of the request.
    *
    * [`patterns_begin`, `patterns_end`) should point to a valid
    * range of elements convertible to `std::string_view`.
    * If the range contains `["news.*", "events.*"]`, the resulting command
    * is `PSUBSCRIBE news.* events.*`.
    *
    * PSUBSCRIBE commands created using this function are tracked
    * to enable PubSub state restoration. After successfully executing
    * the request, the list of subscribed patterns is stored
    * in the connection. Every time a reconnection happens,
    * a suitable `PSUBSCRIBE` command is issued automatically,
    * to restore the subscriptions that were active before the reconnection.
    * 
    * PubSub store restoration only happens when using @ref push_subscribe,
    * @ref push_unsubscribe, @ref push_psubscribe and @ref push_punsubscribe.
    * Use @ref push or @ref push_range to disable it.
    */
   template <class ForwardIt>
   void push_psubscribe(ForwardIt patterns_begin, ForwardIt patterns_end)
   {
      push_range("PSUBSCRIBE", patterns_begin, patterns_end);
   }

   /**
    * @brief Appends a PUNSUBSCRIBE command to the end of the request.
    *
    * If `patterns` contains `{"news.*", "events.*"}`, the resulting command
    * is `PUNSUBSCRIBE news.* events.*`.
    *
    * PUNSUBSCRIBE commands created using this function are tracked
    * to enable PubSub state restoration. After successfully executing
    * the request, the list of subscribed patterns tracked by the
    * connection is updated.
    * 
    * PubSub store restoration only happens when using @ref push_subscribe,
    * @ref push_unsubscribe, @ref push_psubscribe and @ref push_punsubscribe.
    * Use @ref push or @ref push_range to disable it.
    */
   void push_punsubscribe(std::initializer_list<std::string_view> patterns)
   {
      push_punsubscribe(patterns.begin(), patterns.end());
   }

   /**
    * @brief Appends a PUNSUBSCRIBE command to the end of the request.
    *
    * If `patterns` contains `["news.*", "events.*"]`, the resulting command
    * is `PUNSUBSCRIBE news.* events.*`.
    *
    * PUNSUBSCRIBE commands created using this function are tracked
    * to enable PubSub state restoration. After successfully executing
    * the request, the list of subscribed patterns tracked by the
    * connection is updated.
    * 
    * PubSub store restoration only happens when using @ref push_subscribe,
    * @ref push_unsubscribe, @ref push_psubscribe and @ref push_punsubscribe.
    * Use @ref push or @ref push_range to disable it.
    */
   template <class Range>
   void push_punsubscribe(Range&& patterns, decltype(std::cbegin(patterns))* = nullptr)
   {
      push_punsubscribe(std::cbegin(patterns), std::cend(patterns));
   }

   /**
    * @brief Appends a PUNSUBSCRIBE command to the end of the request.
    *
    * [`patterns_begin`, `patterns_end`) should point to a valid
    * range of elements convertible to `std::string_view`.
    * If the range contains `["news.*", "events.*"]`, the resulting command
    * is `PUNSUBSCRIBE news.* events.*`.
    *
    * PUNSUBSCRIBE commands created using this function are tracked
    * to enable PubSub state restoration. After successfully executing
    * the request, the list of subscribed patterns tracked by the
    * connection is updated.
    * 
    * PubSub store restoration only happens when using @ref push_subscribe,
    * @ref push_unsubscribe, @ref push_psubscribe and @ref push_punsubscribe.
    * Use @ref push or @ref push_range to disable it.
    */
   template <class ForwardIt>
   void push_punsubscribe(ForwardIt patterns_begin, ForwardIt patterns_end)
   {
      push_range("PUNSUBSCRIBE", patterns_begin, patterns_end);
   }

   // ===== STRING COMMANDS =====

   void push_get(std::string_view key) { push("GET", key); }

   template <class T>
   void push_set(std::string_view key, const T& value, const set_args& args = {});

   template <class T>
   void push_append(std::string_view key, const T& value)
   {
      push("APPEND", key, value);
   }

   void push_getrange(std::string_view key, int64_t start, int64_t end)
   {
      push("GETRANGE", key, start, end);
   }

   template <class T>
   void push_setrange(std::string_view key, int64_t offset, const T& value)
   {
      push("SETRANGE", key, offset, value);
   }

   void push_strlen(std::string_view key) { push("STRLEN", key); }

   void push_getdel(std::string_view key) { push("GETDEL", key); }

   void push_getex(std::string_view key, getex_args args);

   void push_incr(std::string_view key) { push("INCR", key); }

   void push_incrby(std::string_view key, int64_t increment) { push("INCRBY", key, increment); }

   void push_incrbyfloat(std::string_view key, double increment);

   void push_decr(std::string_view key) { push("DECR", key); }

   void push_decrby(std::string_view key, int64_t decrement) { push("DECRBY", key, decrement); }

   void push_mget(std::initializer_list<std::string_view> keys)
   {
      push_mget(keys.begin(), keys.end());
   }

   template <class Range>
   void push_mget(Range&& keys, decltype(std::cbegin(keys))* = nullptr)
   {
      push_mget(std::cbegin(keys), std::cend(keys));
   }

   template <class ForwardIt>
   void push_mget(ForwardIt keys_begin, ForwardIt keys_end)
   {
      push_range("MGET", keys_begin, keys_end);
   }

   template <class ForwardIt>
   void push_mset(ForwardIt pairs_begin, ForwardIt pairs_end)
   {
      push_range("MSET", pairs_begin, pairs_end);
   }

   template <class Range>
   void push_mset(Range&& pairs, decltype(std::cbegin(pairs))* = nullptr)
   {
      push_mset(std::cbegin(pairs), std::cend(pairs));
   }

   void push_mset(std::initializer_list<std::pair<std::string_view, std::string_view>> pairs)
   {
      push_mset(pairs.begin(), pairs.end());
   }

   template <class ForwardIt>
   void push_msetnx(ForwardIt pairs_begin, ForwardIt pairs_end)
   {
      push_range("MSETNX", pairs_begin, pairs_end);
   }

   template <class Range>
   void push_msetnx(Range&& pairs, decltype(std::cbegin(pairs))* = nullptr)
   {
      push_msetnx(std::cbegin(pairs), std::cend(pairs));
   }

   void push_msetnx(std::initializer_list<std::pair<std::string_view, std::string_view>> pairs)
   {
      push_msetnx(pairs.begin(), pairs.end());
   }

   // ===== HASH COMMANDS =====

   template <class T>
   void push_hset(std::string_view key, std::string_view field, const T& value)
   {
      push("HSET", key, field, value);
   }

   template <class ForwardIt>
   void push_hset(std::string_view key, ForwardIt fields_begin, ForwardIt fields_end)
   {
      push_range("HSET", key, fields_begin, fields_end);
   }

   template <class Range>
   void push_hset(
      std::string_view key,
      Range&& field_values,
      decltype(std::cbegin(field_values))* = nullptr)
   {
      push_hset(key, std::cbegin(field_values), std::cend(field_values));
   }

   void push_hget(std::string_view key, std::string_view field) { push("HGET", key, field); }

   void push_hmget(std::string_view key, std::initializer_list<std::string_view> fields)
   {
      push_hmget(key, fields.begin(), fields.end());
   }

   template <class ForwardIt>
   void push_hmget(std::string_view key, ForwardIt fields_begin, ForwardIt fields_end)
   {
      push_range("HMGET", key, fields_begin, fields_end);
   }

   void push_hgetall(std::string_view key) { push("HGETALL", key); }

   void push_hdel(std::string_view key, std::initializer_list<std::string_view> fields)
   {
      push_hdel(key, fields.begin(), fields.end());
   }

   template <class ForwardIt>
   void push_hdel(std::string_view key, ForwardIt fields_begin, ForwardIt fields_end)
   {
      push_range("HDEL", key, fields_begin, fields_end);
   }

   void push_hexists(std::string_view key, std::string_view field) { push("HEXISTS", key, field); }

   void push_hkeys(std::string_view key) { push("HKEYS", key); }

   void push_hvals(std::string_view key) { push("HVALS", key); }

   void push_hlen(std::string_view key) { push("HLEN", key); }

   void push_hincrby(std::string_view key, std::string_view field, int64_t increment)
   {
      push("HINCRBY", key, field, increment);
   }

   void push_hincrbyfloat(std::string_view key, std::string_view field, double increment)
   {
      push("HINCRBYFLOAT", key, field, increment);
   }

   void push_hscan(std::string_view key, int64_t cursor) { push("HSCAN", key, cursor); }

   void push_hscan(std::string_view key, int64_t cursor, std::string_view pattern, int64_t count)
   {
      push("HSCAN", key, cursor, "MATCH", pattern, "COUNT", count);
   }

   void push_hstrlen(std::string_view key, std::string_view field) { push("HSTRLEN", key, field); }

   void push_hrandfield(std::string_view key) { push("HRANDFIELD", key); }

   void push_hrandfield(std::string_view key, int64_t count) { push("HRANDFIELD", key, count); }

   // ===== LIST COMMANDS =====

   template <class T>
   void push_lpush(std::string_view key, const T& element)
   {
      push("LPUSH", key, element);
   }

   void push_lpush(std::string_view key, std::initializer_list<std::string_view> elements)
   {
      push_lpush(key, elements.begin(), elements.end());
   }

   template <class Range>
   void push_lpush(
      std::string_view key,
      Range&& elements,
      decltype(std::cbegin(elements))* = nullptr)
   {
      push_lpush(key, std::cbegin(elements), std::cend(elements));
   }

   template <class ForwardIt>
   void push_lpush(std::string_view key, ForwardIt elements_begin, ForwardIt elements_end)
   {
      push_range("LPUSH", key, elements_begin, elements_end);
   }

   template <class T>
   void push_lpushx(std::string_view key, const T& element)
   {
      push("LPUSHX", key, element);
   }

   template <class T>
   void push_rpush(std::string_view key, const T& element)
   {
      push("RPUSH", key, element);
   }

   void push_rpush(std::string_view key, std::initializer_list<std::string_view> elements)
   {
      push_rpush(key, elements.begin(), elements.end());
   }

   template <class Range>
   void push_rpush(
      std::string_view key,
      Range&& elements,
      decltype(std::cbegin(elements))* = nullptr)
   {
      push_rpush(key, std::cbegin(elements), std::cend(elements));
   }

   template <class ForwardIt>
   void push_rpush(std::string_view key, ForwardIt elements_begin, ForwardIt elements_end)
   {
      push_range("RPUSH", key, elements_begin, elements_end);
   }

   template <class T>
   void push_rpushx(std::string_view key, const T& element)
   {
      push("RPUSHX", key, element);
   }

   void push_lpop(std::string_view key) { push("LPOP", key); }

   void push_lpop(std::string_view key, int64_t count) { push("LPOP", key, count); }

   void push_rpop(std::string_view key) { push("RPOP", key); }

   void push_rpop(std::string_view key, int64_t count) { push("RPOP", key, count); }

   void push_llen(std::string_view key) { push("LLEN", key); }

   void push_lrange(std::string_view key, int64_t start, int64_t stop)
   {
      push("LRANGE", key, start, stop);
   }

   void push_lindex(std::string_view key, int64_t index) { push("LINDEX", key, index); }

   template <class T>
   void push_lset(std::string_view key, int64_t index, const T& element)
   {
      push("LSET", key, index, element);
   }

   template <class T>
   void push_linsert(std::string_view key, bool before, std::string_view pivot, const T& element)
   {
      push("LINSERT", key, before ? "BEFORE" : "AFTER", pivot, element);
   }

   template <class T>
   void push_lrem(std::string_view key, int64_t count, const T& element)
   {
      push("LREM", key, count, element);
   }

   void push_ltrim(std::string_view key, int64_t start, int64_t stop)
   {
      push("LTRIM", key, start, stop);
   }

   template <class T>
   void push_lpos(std::string_view key, const T& element)
   {
      push("LPOS", key, element);
   }

   void push_blpop(std::string_view key, std::chrono::seconds timeout)
   {
      push("BLPOP", key, timeout.count());
   }

   void push_blpop(std::initializer_list<std::string_view> keys, std::chrono::seconds timeout)
   {
      push_range("BLPOP", keys.begin(), keys.end());
      push_range("BLPOP", std::array{timeout.count()}.begin(), std::array{timeout.count()}.end());
   }

   void push_brpop(std::string_view key, std::chrono::seconds timeout)
   {
      push("BRPOP", key, timeout.count());
   }

   void push_lmove(
      std::string_view source,
      std::string_view destination,
      bool from_left,
      bool to_left)
   {
      push("LMOVE", source, destination, from_left ? "LEFT" : "RIGHT", to_left ? "LEFT" : "RIGHT");
   }

   void push_blmove(
      std::string_view source,
      std::string_view destination,
      bool from_left,
      bool to_left,
      std::chrono::seconds timeout)
   {
      push(
         "BLMOVE",
         source,
         destination,
         from_left ? "LEFT" : "RIGHT",
         to_left ? "LEFT" : "RIGHT",
         timeout.count());
   }

   // ===== SET COMMANDS =====

   template <class T>
   void push_sadd(std::string_view key, const T& member)
   {
      push("SADD", key, member);
   }

   void push_sadd(std::string_view key, std::initializer_list<std::string_view> members)
   {
      push_sadd(key, members.begin(), members.end());
   }

   template <class Range>
   void push_sadd(std::string_view key, Range&& members, decltype(std::cbegin(members))* = nullptr)
   {
      push_sadd(key, std::cbegin(members), std::cend(members));
   }

   template <class ForwardIt>
   void push_sadd(std::string_view key, ForwardIt members_begin, ForwardIt members_end)
   {
      push_range("SADD", key, members_begin, members_end);
   }

   void push_srem(std::string_view key, std::initializer_list<std::string_view> members)
   {
      push_srem(key, members.begin(), members.end());
   }

   template <class Range>
   void push_srem(std::string_view key, Range&& members, decltype(std::cbegin(members))* = nullptr)
   {
      push_srem(key, std::cbegin(members), std::cend(members));
   }

   template <class ForwardIt>
   void push_srem(std::string_view key, ForwardIt members_begin, ForwardIt members_end)
   {
      push_range("SREM", key, members_begin, members_end);
   }

   void push_scard(std::string_view key) { push("SCARD", key); }

   void push_smembers(std::string_view key) { push("SMEMBERS", key); }

   void push_sismember(std::string_view key, std::string_view member)
   {
      push("SISMEMBER", key, member);
   }

   void push_smismember(std::string_view key, std::initializer_list<std::string_view> members)
   {
      push_smismember(key, members.begin(), members.end());
   }

   template <class ForwardIt>
   void push_smismember(std::string_view key, ForwardIt members_begin, ForwardIt members_end)
   {
      push_range("SMISMEMBER", key, members_begin, members_end);
   }

   void push_sinter(std::initializer_list<std::string_view> keys)
   {
      push_sinter(keys.begin(), keys.end());
   }

   template <class ForwardIt>
   void push_sinter(ForwardIt keys_begin, ForwardIt keys_end)
   {
      push_range("SINTER", keys_begin, keys_end);
   }

   void push_sinterstore(std::string_view destination, std::initializer_list<std::string_view> keys)
   {
      push_range("SINTERSTORE", destination, keys.begin(), keys.end());
   }

   void push_sunion(std::initializer_list<std::string_view> keys)
   {
      push_sunion(keys.begin(), keys.end());
   }

   template <class ForwardIt>
   void push_sunion(ForwardIt keys_begin, ForwardIt keys_end)
   {
      push_range("SUNION", keys_begin, keys_end);
   }

   void push_sunionstore(std::string_view destination, std::initializer_list<std::string_view> keys)
   {
      push_range("SUNIONSTORE", destination, keys.begin(), keys.end());
   }

   void push_sdiff(std::initializer_list<std::string_view> keys)
   {
      push_sdiff(keys.begin(), keys.end());
   }

   template <class ForwardIt>
   void push_sdiff(ForwardIt keys_begin, ForwardIt keys_end)
   {
      push_range("SDIFF", keys_begin, keys_end);
   }

   void push_sdiffstore(std::string_view destination, std::initializer_list<std::string_view> keys)
   {
      push_range("SDIFFSTORE", destination, keys.begin(), keys.end());
   }

   void push_spop(std::string_view key) { push("SPOP", key); }

   void push_spop(std::string_view key, int64_t count) { push("SPOP", key, count); }

   void push_srandmember(std::string_view key) { push("SRANDMEMBER", key); }

   void push_srandmember(std::string_view key, int64_t count) { push("SRANDMEMBER", key, count); }

   void push_smove(std::string_view source, std::string_view destination, std::string_view member)
   {
      push("SMOVE", source, destination, member);
   }

   void push_sscan(std::string_view key, int64_t cursor) { push("SSCAN", key, cursor); }

   // ===== SORTED SET COMMANDS =====

   void push_zadd(std::string_view key, double score, std::string_view member)
   {
      push("ZADD", key, score, member);
   }

   template <class ForwardIt>
   void push_zadd(std::string_view key, ForwardIt score_members_begin, ForwardIt score_members_end)
   {
      push_range("ZADD", key, score_members_begin, score_members_end);
   }

   template <class Range>
   void push_zadd(
      std::string_view key,
      Range&& score_members,
      decltype(std::cbegin(score_members))* = nullptr)
   {
      push_zadd(key, std::cbegin(score_members), std::cend(score_members));
   }

   void push_zrem(std::string_view key, std::initializer_list<std::string_view> members)
   {
      push_zrem(key, members.begin(), members.end());
   }

   template <class ForwardIt>
   void push_zrem(std::string_view key, ForwardIt members_begin, ForwardIt members_end)
   {
      push_range("ZREM", key, members_begin, members_end);
   }

   void push_zcard(std::string_view key) { push("ZCARD", key); }

   void push_zcount(std::string_view key, double min, double max) { push("ZCOUNT", key, min, max); }

   void push_zscore(std::string_view key, std::string_view member) { push("ZSCORE", key, member); }

   void push_zmscore(std::string_view key, std::initializer_list<std::string_view> members)
   {
      push_zmscore(key, members.begin(), members.end());
   }

   template <class ForwardIt>
   void push_zmscore(std::string_view key, ForwardIt members_begin, ForwardIt members_end)
   {
      push_range("ZMSCORE", key, members_begin, members_end);
   }

   void push_zrank(std::string_view key, std::string_view member) { push("ZRANK", key, member); }

   void push_zrevrank(std::string_view key, std::string_view member)
   {
      push("ZREVRANK", key, member);
   }

   void push_zrange(std::string_view key, int64_t start, int64_t stop)
   {
      push("ZRANGE", key, start, stop);
   }

   void push_zrange(std::string_view key, int64_t start, int64_t stop, bool withscores)
   {
      if (withscores)
         push("ZRANGE", key, start, stop, "WITHSCORES");
      else
         push("ZRANGE", key, start, stop);
   }

   void push_zrevrange(std::string_view key, int64_t start, int64_t stop)
   {
      push("ZREVRANGE", key, start, stop);
   }

   void push_zrangebyscore(std::string_view key, double min, double max)
   {
      push("ZRANGEBYSCORE", key, min, max);
   }

   void push_zrevrangebyscore(std::string_view key, double max, double min)
   {
      push("ZREVRANGEBYSCORE", key, max, min);
   }

   void push_zincrby(std::string_view key, double increment, std::string_view member)
   {
      push("ZINCRBY", key, increment, member);
   }

   void push_zpopmax(std::string_view key) { push("ZPOPMAX", key); }

   void push_zpopmax(std::string_view key, int64_t count) { push("ZPOPMAX", key, count); }

   void push_zpopmin(std::string_view key) { push("ZPOPMIN", key); }

   void push_zpopmin(std::string_view key, int64_t count) { push("ZPOPMIN", key, count); }

   void push_zrandmember(std::string_view key) { push("ZRANDMEMBER", key); }

   void push_zrandmember(std::string_view key, int64_t count) { push("ZRANDMEMBER", key, count); }

   void push_zscan(std::string_view key, int64_t cursor) { push("ZSCAN", key, cursor); }

   // ===== KEY MANAGEMENT COMMANDS =====

   void push_del(std::string_view key) { push("DEL", key); }

   void push_del(std::initializer_list<std::string_view> keys)
   {
      push_del(keys.begin(), keys.end());
   }

   template <class ForwardIt>
   void push_del(ForwardIt keys_begin, ForwardIt keys_end)
   {
      push_range("DEL", keys_begin, keys_end);
   }

   void push_unlink(std::string_view key) { push("UNLINK", key); }

   void push_unlink(std::initializer_list<std::string_view> keys)
   {
      push_unlink(keys.begin(), keys.end());
   }

   template <class ForwardIt>
   void push_unlink(ForwardIt keys_begin, ForwardIt keys_end)
   {
      push_range("UNLINK", keys_begin, keys_end);
   }

   void push_exists(std::string_view key) { push("EXISTS", key); }

   void push_exists(std::initializer_list<std::string_view> keys)
   {
      push_exists(keys.begin(), keys.end());
   }

   template <class ForwardIt>
   void push_exists(ForwardIt keys_begin, ForwardIt keys_end)
   {
      push_range("EXISTS", keys_begin, keys_end);
   }

   void push_expire(std::string_view key, std::chrono::seconds seconds)
   {
      push("EXPIRE", key, seconds.count());
   }

   void push_expireat(std::string_view key, std::chrono::system_clock::time_point tp)
   {
      auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch())
                          .count();
      push("EXPIREAT", key, timestamp);
   }

   void push_pexpire(std::string_view key, std::chrono::milliseconds ms)
   {
      push("PEXPIRE", key, ms.count());
   }

   void push_pexpireat(std::string_view key, std::chrono::system_clock::time_point tp)
   {
      auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch())
                          .count();
      push("PEXPIREAT", key, timestamp);
   }

   void push_ttl(std::string_view key) { push("TTL", key); }

   void push_pttl(std::string_view key) { push("PTTL", key); }

   void push_persist(std::string_view key) { push("PERSIST", key); }

   void push_keys(std::string_view pattern) { push("KEYS", pattern); }

   void push_randomkey() { push("RANDOMKEY"); }

   void push_rename(std::string_view key, std::string_view newkey) { push("RENAME", key, newkey); }

   void push_renamenx(std::string_view key, std::string_view newkey)
   {
      push("RENAMENX", key, newkey);
   }

   void push_type(std::string_view key) { push("TYPE", key); }

   void push_scan(int64_t cursor) { push("SCAN", cursor); }

   void push_scan(int64_t cursor, std::string_view pattern, int64_t count)
   {
      push("SCAN", cursor, "MATCH", pattern, "COUNT", count);
   }

   void push_copy(std::string_view source, std::string_view destination)
   {
      push("COPY", source, destination);
   }

   void push_copy(std::string_view source, std::string_view destination, bool replace)
   {
      if (replace)
         push("COPY", source, destination, "REPLACE");
      else
         push("COPY", source, destination);
   }

   void push_move(std::string_view key, int64_t db) { push("MOVE", key, db); }

   void push_touch(std::initializer_list<std::string_view> keys)
   {
      push_touch(keys.begin(), keys.end());
   }

   template <class ForwardIt>
   void push_touch(ForwardIt keys_begin, ForwardIt keys_end)
   {
      push_range("TOUCH", keys_begin, keys_end);
   }

   void push_dump(std::string_view key) { push("DUMP", key); }

   template <class T>
   void push_restore(std::string_view key, std::chrono::milliseconds ttl, const T& serialized_value)
   {
      push("RESTORE", key, ttl.count(), serialized_value);
   }

   // ===== BITMAP COMMANDS =====

   void push_setbit(std::string_view key, int64_t offset, int value)
   {
      push("SETBIT", key, offset, value);
   }

   void push_getbit(std::string_view key, int64_t offset) { push("GETBIT", key, offset); }

   void push_bitcount(std::string_view key) { push("BITCOUNT", key); }

   void push_bitcount(std::string_view key, int64_t start, int64_t end)
   {
      push("BITCOUNT", key, start, end);
   }

   void push_bitpos(std::string_view key, int bit) { push("BITPOS", key, bit); }

   void push_bitpos(std::string_view key, int bit, int64_t start, int64_t end)
   {
      push("BITPOS", key, bit, start, end);
   }

   void push_bitop_and(std::string_view destkey, std::initializer_list<std::string_view> keys);

   void push_bitop_or(std::string_view destkey, std::initializer_list<std::string_view> keys);

   void push_bitop_xor(std::string_view destkey, std::initializer_list<std::string_view> keys);

   void push_bitop_not(std::string_view destkey, std::string_view key)
   {
      push("BITOP", "NOT", destkey, key);
   }

   // ===== HYPERLOGLOG COMMANDS =====

   void push_pfadd(std::string_view key, std::initializer_list<std::string_view> elements)
   {
      push_pfadd(key, elements.begin(), elements.end());
   }

   template <class ForwardIt>
   void push_pfadd(std::string_view key, ForwardIt elements_begin, ForwardIt elements_end)
   {
      push_range("PFADD", key, elements_begin, elements_end);
   }

   void push_pfcount(std::string_view key) { push("PFCOUNT", key); }

   void push_pfcount(std::initializer_list<std::string_view> keys)
   {
      push_pfcount(keys.begin(), keys.end());
   }

   template <class ForwardIt>
   void push_pfcount(ForwardIt keys_begin, ForwardIt keys_end)
   {
      push_range("PFCOUNT", keys_begin, keys_end);
   }

   void push_pfmerge(std::string_view destkey, std::initializer_list<std::string_view> sourcekeys)
   {
      push_range("PFMERGE", destkey, sourcekeys.begin(), sourcekeys.end());
   }

   // ===== GEOSPATIAL COMMANDS =====

   void push_geoadd(
      std::string_view key,
      double longitude,
      double latitude,
      std::string_view member)
   {
      push("GEOADD", key, longitude, latitude, member);
   }

   template <class ForwardIt>
   void push_geoadd(
      std::string_view key,
      ForwardIt coord_members_begin,
      ForwardIt coord_members_end)
   {
      push_range("GEOADD", key, coord_members_begin, coord_members_end);
   }

   void push_geodist(std::string_view key, std::string_view member1, std::string_view member2)
   {
      push("GEODIST", key, member1, member2);
   }

   void push_geodist(
      std::string_view key,
      std::string_view member1,
      std::string_view member2,
      std::string_view unit)
   {
      push("GEODIST", key, member1, member2, unit);
   }

   void push_geohash(std::string_view key, std::initializer_list<std::string_view> members)
   {
      push_geohash(key, members.begin(), members.end());
   }

   template <class ForwardIt>
   void push_geohash(std::string_view key, ForwardIt members_begin, ForwardIt members_end)
   {
      push_range("GEOHASH", key, members_begin, members_end);
   }

   void push_geopos(std::string_view key, std::initializer_list<std::string_view> members)
   {
      push_geopos(key, members.begin(), members.end());
   }

   template <class ForwardIt>
   void push_geopos(std::string_view key, ForwardIt members_begin, ForwardIt members_end)
   {
      push_range("GEOPOS", key, members_begin, members_end);
   }

   void push_geosearch(
      std::string_view key,
      double longitude,
      double latitude,
      double radius,
      std::string_view unit)
   {
      push("GEOSEARCH", key, "FROMLONLAT", longitude, latitude, "BYRADIUS", radius, unit);
   }

   // ===== STREAM COMMANDS =====

   template <class ForwardIt>
   void push_xadd(
      std::string_view key,
      std::string_view id,
      ForwardIt fields_begin,
      ForwardIt fields_end)
   {
      push_range("XADD", std::array{key, id}.begin(), std::array{key, id}.end());
      push_range("XADD", fields_begin, fields_end);
   }

   void push_xlen(std::string_view key) { push("XLEN", key); }

   void push_xrange(std::string_view key, std::string_view start, std::string_view end)
   {
      push("XRANGE", key, start, end);
   }

   void push_xrevrange(std::string_view key, std::string_view end, std::string_view start)
   {
      push("XREVRANGE", key, end, start);
   }

   void push_xdel(std::string_view key, std::initializer_list<std::string_view> ids)
   {
      push_xdel(key, ids.begin(), ids.end());
   }

   template <class ForwardIt>
   void push_xdel(std::string_view key, ForwardIt ids_begin, ForwardIt ids_end)
   {
      push_range("XDEL", key, ids_begin, ids_end);
   }

   void push_xtrim(std::string_view key, int64_t maxlen) { push("XTRIM", key, "MAXLEN", maxlen); }

   void push_xread(int64_t count, std::string_view key, std::string_view id)
   {
      push("XREAD", "COUNT", count, "STREAMS", key, id);
   }

   // ===== JSON COMMANDS (RedisJSON module) =====

   template <class T>
   void push_json_set(std::string_view key, std::string_view path, const T& json)
   {
      push("JSON.SET", key, path, json);
   }

   void push_json_get(std::string_view key) { push("JSON.GET", key); }

   void push_json_get(std::string_view key, std::string_view path) { push("JSON.GET", key, path); }

   void push_json_del(std::string_view key) { push("JSON.DEL", key); }

   void push_json_del(std::string_view key, std::string_view path) { push("JSON.DEL", key, path); }

   void push_json_mget(std::initializer_list<std::string_view> keys, std::string_view path)
   {
      push_range("JSON.MGET", keys.begin(), keys.end());
      push("JSON.MGET", path);
   }

   void push_json_type(std::string_view key) { push("JSON.TYPE", key); }

   void push_json_type(std::string_view key, std::string_view path)
   {
      push("JSON.TYPE", key, path);
   }

private:
   void check_cmd(std::string_view cmd)
   {
      ++commands_;

      if (!detail::has_response(cmd))
         ++expected_responses_;

      if (cmd == "HELLO")
         has_hello_priority_ = cfg_.hello_with_priority;
   }

   config cfg_;
   std::string payload_;
   std::size_t commands_ = 0;
   std::size_t expected_responses_ = 0;
   bool has_hello_priority_ = false;

   friend struct detail::request_access;
};

namespace detail {

struct request_access {
   inline static void set_priority(request& r, bool value) { r.has_hello_priority_ = value; }
   inline static bool has_priority(const request& r) { return r.has_hello_priority_; }
};

// Creates a HELLO 3 request
request make_hello_request();

}  // namespace detail

}  // namespace boost::redis

#endif  // BOOST_REDIS_REQUEST_HPP
