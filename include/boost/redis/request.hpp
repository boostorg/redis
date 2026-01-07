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

   /**
    * @brief Appends a GET command to the end of the request.
    *
    * If `key` is `"mykey"`, the resulting command is `GET mykey`.
    *
    * @param key The key to retrieve.
    */
   void push_get(std::string_view key) { push("GET", key); }

   /**
    * @brief Appends a SET command to the end of the request.
    *
    * If `key` is `"mykey"` and `value` is `"myvalue"`, the resulting command is `SET mykey myvalue`.
    * Optional arguments can be provided via the `args` parameter.
    *
    * @param key The key to set.
    * @param value The value to set.
    * @param args Optional arguments for SET (condition, expiry, GET flag).
    */
   template <class T>
   void push_set(std::string_view key, const T& value, const set_args& args = {});

   /**
    * @brief Appends an APPEND command to the end of the request.
    *
    * If `key` is `"mykey"` and `value` is `"hello"`, the resulting command is `APPEND mykey hello`.
    *
    * @param key The key to append to.
    * @param value The value to append.
    */
   template <class T>
   void push_append(std::string_view key, const T& value)
   {
      push("APPEND", key, value);
   }

   /**
    * @brief Appends a GETRANGE command to the end of the request.
    *
    * If `key` is `"mykey"`, `start` is `0`, and `end` is `3`, the resulting command is `GETRANGE mykey 0 3`.
    *
    * @param key The key to get a substring from.
    * @param start The start offset (inclusive).
    * @param end The end offset (inclusive).
    */
   void push_getrange(std::string_view key, int64_t start, int64_t end)
   {
      push("GETRANGE", key, start, end);
   }

   /**
    * @brief Appends a SETRANGE command to the end of the request.
    *
    * If `key` is `"mykey"`, `offset` is `6`, and `value` is `"world"`, the resulting command is `SETRANGE mykey 6 world`.
    *
    * @param key The key to modify.
    * @param offset The offset at which to start overwriting.
    * @param value The value to write.
    */
   template <class T>
   void push_setrange(std::string_view key, int64_t offset, const T& value)
   {
      push("SETRANGE", key, offset, value);
   }

   /**
    * @brief Appends a STRLEN command to the end of the request.
    *
    * If `key` is `"mykey"`, the resulting command is `STRLEN mykey`.
    *
    * @param key The key to get the length of.
    */
   void push_strlen(std::string_view key) { push("STRLEN", key); }

   /**
    * @brief Appends a GETDEL command to the end of the request.
    *
    * If `key` is `"mykey"`, the resulting command is `GETDEL mykey`.
    *
    * @param key The key to get and delete.
    */
   void push_getdel(std::string_view key) { push("GETDEL", key); }

   /**
    * @brief Appends a GETEX command to the end of the request.
    *
    * If `key` is `"mykey"` and `args` specifies `EX 10`, the resulting command is `GETEX mykey EX 10`.
    *
    * @param key The key to get.
    * @param args The expiration arguments.
    */
   void push_getex(std::string_view key, getex_args args);

   /**
    * @brief Appends an INCR command to the end of the request.
    *
    * If `key` is `"counter"`, the resulting command is `INCR counter`.
    *
    * @param key The key to increment.
    */
   void push_incr(std::string_view key) { push("INCR", key); }

   /**
    * @brief Appends an INCRBY command to the end of the request.
    *
    * If `key` is `"counter"` and `increment` is `5`, the resulting command is `INCRBY counter 5`.
    *
    * @param key The key to increment.
    * @param increment The amount to increment by.
    */
   void push_incrby(std::string_view key, int64_t increment) { push("INCRBY", key, increment); }

   /**
    * @brief Appends an INCRBYFLOAT command to the end of the request.
    *
    * If `key` is `"temperature"` and `increment` is `2.5`, the resulting command is `INCRBYFLOAT temperature 2.5`.
    *
    * @param key The key to increment.
    * @param increment The floating-point amount to increment by.
    */
   void push_incrbyfloat(std::string_view key, double increment);

   /**
    * @brief Appends a DECR command to the end of the request.
    *
    * If `key` is `"counter"`, the resulting command is `DECR counter`.
    *
    * @param key The key to decrement.
    */
   void push_decr(std::string_view key) { push("DECR", key); }

   /**
    * @brief Appends a DECRBY command to the end of the request.
    *
    * If `key` is `"counter"` and `decrement` is `3`, the resulting command is `DECRBY counter 3`.
    *
    * @param key The key to decrement.
    * @param decrement The amount to decrement by.
    */
   void push_decrby(std::string_view key, int64_t decrement) { push("DECRBY", key, decrement); }

   /**
    * @brief Appends an MGET command to the end of the request.
    *
    * If `keys` contains `{"key1", "key2"}`, the resulting command is `MGET key1 key2`.
    *
    * @param keys The keys to retrieve.
    */
   void push_mget(std::initializer_list<std::string_view> keys)
   {
      push_mget(keys.begin(), keys.end());
   }

   /**
    * @brief Appends an MGET command to the end of the request.
    *
    * If `keys` contains `["key1", "key2"]`, the resulting command is `MGET key1 key2`.
    *
    * @param keys The keys to retrieve.
    */
   template <class Range>
   void push_mget(Range&& keys, decltype(std::cbegin(keys))* = nullptr)
   {
      push_mget(std::cbegin(keys), std::cend(keys));
   }

   /**
    * @brief Appends an MGET command to the end of the request.
    *
    * [`keys_begin`, `keys_end`) should point to a valid range of elements convertible to `std::string_view`.
    * If the range contains `["key1", "key2"]`, the resulting command is `MGET key1 key2`.
    *
    * @param keys_begin Iterator to the beginning of the keys.
    * @param keys_end Iterator to the end of the keys.
    */
   template <class ForwardIt>
   void push_mget(ForwardIt keys_begin, ForwardIt keys_end)
   {
      push_range("MGET", keys_begin, keys_end);
   }

   /**
    * @brief Appends an MSET command to the end of the request.
    *
    * [`pairs_begin`, `pairs_end`) should point to a valid range of key-value pairs.
    * If the range contains `[{"key1", "val1"}, {"key2", "val2"}]`, the resulting command is `MSET key1 val1 key2 val2`.
    *
    * @param pairs_begin Iterator to the beginning of the key-value pairs.
    * @param pairs_end Iterator to the end of the key-value pairs.
    */
   template <class ForwardIt>
   void push_mset(ForwardIt pairs_begin, ForwardIt pairs_end)
   {
      push_range("MSET", pairs_begin, pairs_end);
   }

   /**
    * @brief Appends an MSET command to the end of the request.
    *
    * If `pairs` contains `[{"key1", "val1"}, {"key2", "val2"}]`, the resulting command is `MSET key1 val1 key2 val2`.
    *
    * @param pairs The key-value pairs to set.
    */
   template <class Range>
   void push_mset(Range&& pairs, decltype(std::cbegin(pairs))* = nullptr)
   {
      push_mset(std::cbegin(pairs), std::cend(pairs));
   }

   /**
    * @brief Appends an MSET command to the end of the request.
    *
    * If `pairs` contains `{{"key1", "val1"}, {"key2", "val2"}}`, the resulting command is `MSET key1 val1 key2 val2`.
    *
    * @param pairs The key-value pairs to set.
    */
   void push_mset(std::initializer_list<std::pair<std::string_view, std::string_view>> pairs)
   {
      push_mset(pairs.begin(), pairs.end());
   }

   /**
    * @brief Appends an MSETNX command to the end of the request.
    *
    * [`pairs_begin`, `pairs_end`) should point to a valid range of key-value pairs.
    * If the range contains `[{"key1", "val1"}, {"key2", "val2"}]`, the resulting command is `MSETNX key1 val1 key2 val2`.
    *
    * @param pairs_begin Iterator to the beginning of the key-value pairs.
    * @param pairs_end Iterator to the end of the key-value pairs.
    */
   template <class ForwardIt>
   void push_msetnx(ForwardIt pairs_begin, ForwardIt pairs_end)
   {
      push_range("MSETNX", pairs_begin, pairs_end);
   }

   /**
    * @brief Appends an MSETNX command to the end of the request.
    *
    * If `pairs` contains `[{"key1", "val1"}, {"key2", "val2"}]`, the resulting command is `MSETNX key1 val1 key2 val2`.
    *
    * @param pairs The key-value pairs to set.
    */
   template <class Range>
   void push_msetnx(Range&& pairs, decltype(std::cbegin(pairs))* = nullptr)
   {
      push_msetnx(std::cbegin(pairs), std::cend(pairs));
   }

   /**
    * @brief Appends an MSETNX command to the end of the request.
    *
    * If `pairs` contains `{{"key1", "val1"}, {"key2", "val2"}}`, the resulting command is `MSETNX key1 val1 key2 val2`.
    *
    * @param pairs The key-value pairs to set.
    */
   void push_msetnx(std::initializer_list<std::pair<std::string_view, std::string_view>> pairs)
   {
      push_msetnx(pairs.begin(), pairs.end());
   }

   // ===== HASH COMMANDS =====

   /**
    * @brief Appends an HSET command to the end of the request.
    *
    * If `key` is `"myhash"`, `field` is `"field1"`, and `value` is `"value1"`, the resulting command is `HSET myhash field1 value1`.
    *
    * @param key The hash key.
    * @param field The field name.
    * @param value The field value.
    */
   template <class T>
   void push_hset(std::string_view key, std::string_view field, const T& value)
   {
      push("HSET", key, field, value);
   }

   /**
    * @brief Appends an HSET command to the end of the request.
    *
    * [`fields_begin`, `fields_end`) should point to a valid range of field-value pairs.
    * If the range contains `[{"field1", "val1"}, {"field2", "val2"}]`, the resulting command is `HSET key field1 val1 field2 val2`.
    *
    * @param key The hash key.
    * @param fields_begin Iterator to the beginning of the field-value pairs.
    * @param fields_end Iterator to the end of the field-value pairs.
    */
   template <class ForwardIt>
   void push_hset(std::string_view key, ForwardIt fields_begin, ForwardIt fields_end)
   {
      push_range("HSET", key, fields_begin, fields_end);
   }

   /**
    * @brief Appends an HSET command to the end of the request.
    *
    * If `field_values` contains `[{"field1", "val1"}, {"field2", "val2"}]`, the resulting command is `HSET key field1 val1 field2 val2`.
    *
    * @param key The hash key.
    * @param field_values The field-value pairs to set.
    */
   template <class Range>
   void push_hset(
      std::string_view key,
      Range&& field_values,
      decltype(std::cbegin(field_values))* = nullptr)
   {
      push_hset(key, std::cbegin(field_values), std::cend(field_values));
   }

   /**
    * @brief Appends an HGET command to the end of the request.
    *
    * If `key` is `"myhash"` and `field` is `"field1"`, the resulting command is `HGET myhash field1`.
    *
    * @param key The hash key.
    * @param field The field name.
    */
   void push_hget(std::string_view key, std::string_view field) { push("HGET", key, field); }

   /**
    * @brief Appends an HMGET command to the end of the request.
    *
    * If `key` is `"myhash"` and `fields` contains `{"field1", "field2"}`, the resulting command is `HMGET myhash field1 field2`.
    *
    * @param key The hash key.
    * @param fields The field names to retrieve.
    */
   void push_hmget(std::string_view key, std::initializer_list<std::string_view> fields)
   {
      push_hmget(key, fields.begin(), fields.end());
   }

   /**
    * @brief Appends an HMGET command to the end of the request.
    *
    * [`fields_begin`, `fields_end`) should point to a valid range of field names.
    * If the range contains `["field1", "field2"]`, the resulting command is `HMGET key field1 field2`.
    *
    * @param key The hash key.
    * @param fields_begin Iterator to the beginning of the field names.
    * @param fields_end Iterator to the end of the field names.
    */
   template <class ForwardIt>
   void push_hmget(std::string_view key, ForwardIt fields_begin, ForwardIt fields_end)
   {
      push_range("HMGET", key, fields_begin, fields_end);
   }

   /**
    * @brief Appends an HGETALL command to the end of the request.
    *
    * If `key` is `"myhash"`, the resulting command is `HGETALL myhash`.
    *
    * @param key The hash key.
    */
   void push_hgetall(std::string_view key) { push("HGETALL", key); }

   /**
    * @brief Appends an HDEL command to the end of the request.
    *
    * If `key` is `"myhash"` and `fields` contains `{"field1", "field2"}`, the resulting command is `HDEL myhash field1 field2`.
    *
    * @param key The hash key.
    * @param fields The field names to delete.
    */
   void push_hdel(std::string_view key, std::initializer_list<std::string_view> fields)
   {
      push_hdel(key, fields.begin(), fields.end());
   }

   /**
    * @brief Appends an HDEL command to the end of the request.
    *
    * [`fields_begin`, `fields_end`) should point to a valid range of field names.
    * If the range contains `["field1", "field2"]`, the resulting command is `HDEL key field1 field2`.
    *
    * @param key The hash key.
    * @param fields_begin Iterator to the beginning of the field names.
    * @param fields_end Iterator to the end of the field names.
    */
   template <class ForwardIt>
   void push_hdel(std::string_view key, ForwardIt fields_begin, ForwardIt fields_end)
   {
      push_range("HDEL", key, fields_begin, fields_end);
   }

   /**
    * @brief Appends an HEXISTS command to the end of the request.
    *
    * If `key` is `"myhash"` and `field` is `"field1"`, the resulting command is `HEXISTS myhash field1`.
    *
    * @param key The hash key.
    * @param field The field name.
    */
   void push_hexists(std::string_view key, std::string_view field) { push("HEXISTS", key, field); }

   /**
    * @brief Appends an HKEYS command to the end of the request.
    *
    * If `key` is `"myhash"`, the resulting command is `HKEYS myhash`.
    *
    * @param key The hash key.
    */
   void push_hkeys(std::string_view key) { push("HKEYS", key); }

   /**
    * @brief Appends an HVALS command to the end of the request.
    *
    * If `key` is `"myhash"`, the resulting command is `HVALS myhash`.
    *
    * @param key The hash key.
    */
   void push_hvals(std::string_view key) { push("HVALS", key); }

   /**
    * @brief Appends an HLEN command to the end of the request.
    *
    * If `key` is `"myhash"`, the resulting command is `HLEN myhash`.
    *
    * @param key The hash key.
    */
   void push_hlen(std::string_view key) { push("HLEN", key); }

   /**
    * @brief Appends an HINCRBY command to the end of the request.
    *
    * If `key` is `"myhash"`, `field` is `"counter"`, and `increment` is `5`, the resulting command is `HINCRBY myhash counter 5`.
    *
    * @param key The hash key.
    * @param field The field name.
    * @param increment The amount to increment by.
    */
   void push_hincrby(std::string_view key, std::string_view field, int64_t increment)
   {
      push("HINCRBY", key, field, increment);
   }

   /**
    * @brief Appends an HINCRBYFLOAT command to the end of the request.
    *
    * If `key` is `"myhash"`, `field` is `"price"`, and `increment` is `2.5`, the resulting command is `HINCRBYFLOAT myhash price 2.5`.
    *
    * @param key The hash key.
    * @param field The field name.
    * @param increment The floating-point amount to increment by.
    */
   void push_hincrbyfloat(std::string_view key, std::string_view field, double increment);

   /**
    * @brief Appends an HSCAN command to the end of the request.
    *
    * If `key` is `"myhash"` and `cursor` is `0`, the resulting command is `HSCAN myhash 0`.
    *
    * @param key The hash key.
    * @param cursor The cursor position.
    */
   void push_hscan(std::string_view key, int64_t cursor) { push("HSCAN", key, cursor); }

   /**
    * @brief Appends an HSCAN command to the end of the request.
    *
    * If `key` is `"myhash"`, `cursor` is `0`, `pattern` is `"field*"`, and `count` is `10`, the resulting command is `HSCAN myhash 0 MATCH field* COUNT 10`.
    *
    * @param key The hash key.
    * @param cursor The cursor position.
    * @param pattern The pattern to match field names against.
    * @param count The number of elements to return per iteration.
    */
   void push_hscan(std::string_view key, int64_t cursor, std::string_view pattern, int64_t count)
   {
      push("HSCAN", key, cursor, "MATCH", pattern, "COUNT", count);
   }

   /**
    * @brief Appends an HSTRLEN command to the end of the request.
    *
    * If `key` is `"myhash"` and `field` is `"field1"`, the resulting command is `HSTRLEN myhash field1`.
    *
    * @param key The hash key.
    * @param field The field name.
    */
   void push_hstrlen(std::string_view key, std::string_view field) { push("HSTRLEN", key, field); }

   /**
    * @brief Appends an HRANDFIELD command to the end of the request.
    *
    * If `key` is `"myhash"`, the resulting command is `HRANDFIELD myhash`.
    *
    * @param key The hash key.
    */
   void push_hrandfield(std::string_view key) { push("HRANDFIELD", key); }

   /**
    * @brief Appends an HRANDFIELD command to the end of the request.
    *
    * If `key` is `"myhash"` and `count` is `2`, the resulting command is `HRANDFIELD myhash 2`.
    *
    * @param key The hash key.
    * @param count The number of random fields to return.
    */
   void push_hrandfield(std::string_view key, int64_t count) { push("HRANDFIELD", key, count); }

   // ===== LIST COMMANDS =====

   /**
    * @brief Appends an LPUSH command to the end of the request.
    *
    * If `key` is `"mylist"` and `element` is `"value1"`, the resulting command is `LPUSH mylist value1`.
    *
    * @param key The list key.
    * @param element The element to prepend to the list.
    */
   template <class T>
   void push_lpush(std::string_view key, const T& element)
   {
      push("LPUSH", key, element);
   }

   /**
    * @brief Appends an LPUSH command to the end of the request.
    *
    * If `key` is `"mylist"` and `elements` contains `{"val1", "val2"}`, the resulting command is `LPUSH mylist val1 val2`.
    *
    * @param key The list key.
    * @param elements The elements to prepend to the list.
    */
   void push_lpush(std::string_view key, std::initializer_list<std::string_view> elements)
   {
      push_lpush(key, elements.begin(), elements.end());
   }

   /**
    * @brief Appends an LPUSH command to the end of the request.
    *
    * If `key` is `"mylist"` and `elements` contains `["val1", "val2"]`, the resulting command is `LPUSH mylist val1 val2`.
    *
    * @param key The list key.
    * @param elements The elements to prepend to the list.
    */
   template <class Range>
   void push_lpush(
      std::string_view key,
      Range&& elements,
      decltype(std::cbegin(elements))* = nullptr)
   {
      push_lpush(key, std::cbegin(elements), std::cend(elements));
   }

   /**
    * @brief Appends an LPUSH command to the end of the request.
    *
    * [`elements_begin`, `elements_end`) should point to a valid range of elements.
    * If the range contains `["val1", "val2"]`, the resulting command is `LPUSH key val1 val2`.
    *
    * @param key The list key.
    * @param elements_begin Iterator to the beginning of the elements.
    * @param elements_end Iterator to the end of the elements.
    */
   template <class ForwardIt>
   void push_lpush(std::string_view key, ForwardIt elements_begin, ForwardIt elements_end)
   {
      push_range("LPUSH", key, elements_begin, elements_end);
   }

   /**
    * @brief Appends an LPUSHX command to the end of the request.
    *
    * If `key` is `"mylist"` and `element` is `"value1"`, the resulting command is `LPUSHX mylist value1`.
    * LPUSHX only inserts if the key already exists.
    *
    * @param key The list key.
    * @param element The element to prepend to the list.
    */
   template <class T>
   void push_lpushx(std::string_view key, const T& element)
   {
      push("LPUSHX", key, element);
   }

   /**
    * @brief Appends an RPUSH command to the end of the request.
    *
    * If `key` is `"mylist"` and `element` is `"value1"`, the resulting command is `RPUSH mylist value1`.
    *
    * @param key The list key.
    * @param element The element to append to the list.
    */
   template <class T>
   void push_rpush(std::string_view key, const T& element)
   {
      push("RPUSH", key, element);
   }

   /**
    * @brief Appends an RPUSH command to the end of the request.
    *
    * If `key` is `"mylist"` and `elements` contains `{"val1", "val2"}`, the resulting command is `RPUSH mylist val1 val2`.
    *
    * @param key The list key.
    * @param elements The elements to append to the list.
    */
   void push_rpush(std::string_view key, std::initializer_list<std::string_view> elements)
   {
      push_rpush(key, elements.begin(), elements.end());
   }

   /**
    * @brief Appends an RPUSH command to the end of the request.
    *
    * If `key` is `"mylist"` and `elements` contains `["val1", "val2"]`, the resulting command is `RPUSH mylist val1 val2`.
    *
    * @param key The list key.
    * @param elements The elements to append to the list.
    */
   template <class Range>
   void push_rpush(
      std::string_view key,
      Range&& elements,
      decltype(std::cbegin(elements))* = nullptr)
   {
      push_rpush(key, std::cbegin(elements), std::cend(elements));
   }

   /**
    * @brief Appends an RPUSH command to the end of the request.
    *
    * [`elements_begin`, `elements_end`) should point to a valid range of elements.
    * If the range contains `["val1", "val2"]`, the resulting command is `RPUSH key val1 val2`.
    *
    * @param key The list key.
    * @param elements_begin Iterator to the beginning of the elements.
    * @param elements_end Iterator to the end of the elements.
    */
   template <class ForwardIt>
   void push_rpush(std::string_view key, ForwardIt elements_begin, ForwardIt elements_end)
   {
      push_range("RPUSH", key, elements_begin, elements_end);
   }

   /**
    * @brief Appends an RPUSHX command to the end of the request.
    *
    * If `key` is `"mylist"` and `element` is `"value1"`, the resulting command is `RPUSHX mylist value1`.
    * RPUSHX only inserts if the key already exists.
    *
    * @param key The list key.
    * @param element The element to append to the list.
    */
   template <class T>
   void push_rpushx(std::string_view key, const T& element)
   {
      push("RPUSHX", key, element);
   }

   /**
    * @brief Appends an LPOP command to the end of the request.
    *
    * If `key` is `"mylist"`, the resulting command is `LPOP mylist`.
    *
    * @param key The list key.
    */
   void push_lpop(std::string_view key) { push("LPOP", key); }

   /**
    * @brief Appends an LPOP command to the end of the request.
    *
    * If `key` is `"mylist"` and `count` is `2`, the resulting command is `LPOP mylist 2`.
    *
    * @param key The list key.
    * @param count The number of elements to pop.
    */
   void push_lpop(std::string_view key, int64_t count) { push("LPOP", key, count); }

   /**
    * @brief Appends an RPOP command to the end of the request.
    *
    * If `key` is `"mylist"`, the resulting command is `RPOP mylist`.
    *
    * @param key The list key.
    */
   void push_rpop(std::string_view key) { push("RPOP", key); }

   /**
    * @brief Appends an RPOP command to the end of the request.
    *
    * If `key` is `"mylist"` and `count` is `2`, the resulting command is `RPOP mylist 2`.
    *
    * @param key The list key.
    * @param count The number of elements to pop.
    */
   void push_rpop(std::string_view key, int64_t count) { push("RPOP", key, count); }

   /**
    * @brief Appends an LLEN command to the end of the request.
    *
    * If `key` is `"mylist"`, the resulting command is `LLEN mylist`.
    *
    * @param key The list key.
    */
   void push_llen(std::string_view key) { push("LLEN", key); }

   /**
    * @brief Appends an LRANGE command to the end of the request.
    *
    * If `key` is `"mylist"`, `start` is `0`, and `stop` is `10`, the resulting command is `LRANGE mylist 0 10`.
    *
    * @param key The list key.
    * @param start The start index (inclusive).
    * @param stop The stop index (inclusive).
    */
   void push_lrange(std::string_view key, int64_t start, int64_t stop)
   {
      push("LRANGE", key, start, stop);
   }

   /**
    * @brief Appends an LINDEX command to the end of the request.
    *
    * If `key` is `"mylist"` and `index` is `2`, the resulting command is `LINDEX mylist 2`.
    *
    * @param key The list key.
    * @param index The index of the element to retrieve.
    */
   void push_lindex(std::string_view key, int64_t index) { push("LINDEX", key, index); }

   /**
    * @brief Appends an LSET command to the end of the request.
    *
    * If `key` is `"mylist"`, `index` is `2`, and `element` is `"newval"`, the resulting command is `LSET mylist 2 newval`.
    *
    * @param key The list key.
    * @param index The index of the element to set.
    * @param element The new element value.
    */
   template <class T>
   void push_lset(std::string_view key, int64_t index, const T& element)
   {
      push("LSET", key, index, element);
   }

   /**
    * @brief Appends an LINSERT command to the end of the request.
    *
    * If `key` is `"mylist"`, `before` is `true`, `pivot` is `"pivot"`, and `element` is `"new"`, the resulting command is `LINSERT mylist BEFORE pivot new`.
    *
    * @param key The list key.
    * @param before If true, insert before the pivot; otherwise, insert after.
    * @param pivot The pivot element.
    * @param element The element to insert.
    */
   template <class T>
   void push_linsert(std::string_view key, bool before, std::string_view pivot, const T& element)
   {
      push("LINSERT", key, before ? "BEFORE" : "AFTER", pivot, element);
   }

   /**
    * @brief Appends an LREM command to the end of the request.
    *
    * If `key` is `"mylist"`, `count` is `2`, and `element` is `"value"`, the resulting command is `LREM mylist 2 value`.
    *
    * @param key The list key.
    * @param count The number of occurrences to remove (positive: from head, negative: from tail, zero: all).
    * @param element The element to remove.
    */
   template <class T>
   void push_lrem(std::string_view key, int64_t count, const T& element)
   {
      push("LREM", key, count, element);
   }

   /**
    * @brief Appends an LTRIM command to the end of the request.
    *
    * If `key` is `"mylist"`, `start` is `0`, and `stop` is `2`, the resulting command is `LTRIM mylist 0 2`.
    *
    * @param key The list key.
    * @param start The start index (inclusive).
    * @param stop The stop index (inclusive).
    */
   void push_ltrim(std::string_view key, int64_t start, int64_t stop)
   {
      push("LTRIM", key, start, stop);
   }

   /**
    * @brief Appends an LPOS command to the end of the request.
    *
    * If `key` is `"mylist"` and `element` is `"value"`, the resulting command is `LPOS mylist value`.
    *
    * @param key The list key.
    * @param element The element to find.
    */
   template <class T>
   void push_lpos(std::string_view key, const T& element)
   {
      push("LPOS", key, element);
   }

   /**
    * @brief Appends a BLPOP command to the end of the request.
    *
    * If `key` is `"mylist"` and `timeout` is `5` seconds, the resulting command is `BLPOP mylist 5`.
    *
    * @param key The list key.
    * @param timeout The timeout in seconds.
    */
   void push_blpop(std::string_view key, std::chrono::seconds timeout)
   {
      push("BLPOP", key, timeout.count());
   }

   /**
    * @brief Appends a BLPOP command to the end of the request.
    *
    * If `keys` contains `{"list1", "list2"}` and `timeout` is `5` seconds, the resulting command is `BLPOP list1 list2 5`.
    *
    * @param keys The list keys.
    * @param timeout The timeout in seconds.
    */
   void push_blpop(std::initializer_list<std::string_view> keys, std::chrono::seconds timeout)
   {
      push_range("BLPOP", keys.begin(), keys.end());
      push_range("BLPOP", std::array{timeout.count()}.begin(), std::array{timeout.count()}.end());
   }

   /**
    * @brief Appends a BRPOP command to the end of the request.
    *
    * If `key` is `"mylist"` and `timeout` is `5` seconds, the resulting command is `BRPOP mylist 5`.
    *
    * @param key The list key.
    * @param timeout The timeout in seconds.
    */
   void push_brpop(std::string_view key, std::chrono::seconds timeout)
   {
      push("BRPOP", key, timeout.count());
   }

   /**
    * @brief Appends an LMOVE command to the end of the request.
    *
    * If `source` is `"list1"`, `destination` is `"list2"`, `from_left` is `true`, and `to_left` is `false`, the resulting command is `LMOVE list1 list2 LEFT RIGHT`.
    *
    * @param source The source list key.
    * @param destination The destination list key.
    * @param from_left If true, pop from the left of the source list; otherwise, pop from the right.
    * @param to_left If true, push to the left of the destination list; otherwise, push to the right.
    */
   void push_lmove(
      std::string_view source,
      std::string_view destination,
      bool from_left,
      bool to_left)
   {
      push("LMOVE", source, destination, from_left ? "LEFT" : "RIGHT", to_left ? "LEFT" : "RIGHT");
   }

   /**
    * @brief Appends a BLMOVE command to the end of the request.
    *
    * If `source` is `"list1"`, `destination` is `"list2"`, `from_left` is `true`, `to_left` is `false`, and `timeout` is `5` seconds, the resulting command is `BLMOVE list1 list2 LEFT RIGHT 5`.
    *
    * @param source The source list key.
    * @param destination The destination list key.
    * @param from_left If true, pop from the left of the source list; otherwise, pop from the right.
    * @param to_left If true, push to the left of the destination list; otherwise, push to the right.
    * @param timeout The timeout in seconds.
    */
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

   /**
    * @brief Appends an SADD command to the end of the request.
    *
    * If `key` is `"myset"` and `member` is `"value1"`, the resulting command is `SADD myset value1`.
    *
    * @param key The set key.
    * @param member The member to add to the set.
    */
   template <class T>
   void push_sadd(std::string_view key, const T& member)
   {
      push("SADD", key, member);
   }

   /**
    * @brief Appends an SADD command to the end of the request.
    *
    * If `key` is `"myset"` and `members` contains `{"val1", "val2"}`, the resulting command is `SADD myset val1 val2`.
    *
    * @param key The set key.
    * @param members The members to add to the set.
    */
   void push_sadd(std::string_view key, std::initializer_list<std::string_view> members)
   {
      push_sadd(key, members.begin(), members.end());
   }

   /**
    * @brief Appends an SADD command to the end of the request.
    *
    * If `key` is `"myset"` and `members` contains `["val1", "val2"]`, the resulting command is `SADD myset val1 val2`.
    *
    * @param key The set key.
    * @param members The members to add to the set.
    */
   template <class Range>
   void push_sadd(std::string_view key, Range&& members, decltype(std::cbegin(members))* = nullptr)
   {
      push_sadd(key, std::cbegin(members), std::cend(members));
   }

   /**
    * @brief Appends an SADD command to the end of the request.
    *
    * [`members_begin`, `members_end`) should point to a valid range of members.
    * If the range contains `["val1", "val2"]`, the resulting command is `SADD key val1 val2`.
    *
    * @param key The set key.
    * @param members_begin Iterator to the beginning of the members.
    * @param members_end Iterator to the end of the members.
    */
   template <class ForwardIt>
   void push_sadd(std::string_view key, ForwardIt members_begin, ForwardIt members_end)
   {
      push_range("SADD", key, members_begin, members_end);
   }

   /**
    * @brief Appends an SREM command to the end of the request.
    *
    * If `key` is `"myset"` and `members` contains `{"val1", "val2"}`, the resulting command is `SREM myset val1 val2`.
    *
    * @param key The set key.
    * @param members The members to remove from the set.
    */
   void push_srem(std::string_view key, std::initializer_list<std::string_view> members)
   {
      push_srem(key, members.begin(), members.end());
   }

   /**
    * @brief Appends an SREM command to the end of the request.
    *
    * If `key` is `"myset"` and `members` contains `["val1", "val2"]`, the resulting command is `SREM myset val1 val2`.
    *
    * @param key The set key.
    * @param members The members to remove from the set.
    */
   template <class Range>
   void push_srem(std::string_view key, Range&& members, decltype(std::cbegin(members))* = nullptr)
   {
      push_srem(key, std::cbegin(members), std::cend(members));
   }

   /**
    * @brief Appends an SREM command to the end of the request.
    *
    * [`members_begin`, `members_end`) should point to a valid range of members.
    * If the range contains `["val1", "val2"]`, the resulting command is `SREM key val1 val2`.
    *
    * @param key The set key.
    * @param members_begin Iterator to the beginning of the members.
    * @param members_end Iterator to the end of the members.
    */
   template <class ForwardIt>
   void push_srem(std::string_view key, ForwardIt members_begin, ForwardIt members_end)
   {
      push_range("SREM", key, members_begin, members_end);
   }

   /**
    * @brief Appends an SCARD command to the end of the request.
    *
    * If `key` is `"myset"`, the resulting command is `SCARD myset`.
    *
    * @param key The set key.
    */
   void push_scard(std::string_view key) { push("SCARD", key); }

   /**
    * @brief Appends an SMEMBERS command to the end of the request.
    *
    * If `key` is `"myset"`, the resulting command is `SMEMBERS myset`.
    *
    * @param key The set key.
    */
   void push_smembers(std::string_view key) { push("SMEMBERS", key); }

   /**
    * @brief Appends an SISMEMBER command to the end of the request.
    *
    * If `key` is `"myset"` and `member` is `"value1"`, the resulting command is `SISMEMBER myset value1`.
    *
    * @param key The set key.
    * @param member The member to check for membership.
    */
   void push_sismember(std::string_view key, std::string_view member)
   {
      push("SISMEMBER", key, member);
   }

   /**
    * @brief Appends an SMISMEMBER command to the end of the request.
    *
    * If `key` is `"myset"` and `members` contains `{"val1", "val2"}`, the resulting command is `SMISMEMBER myset val1 val2`.
    *
    * @param key The set key.
    * @param members The members to check for membership.
    */
   void push_smismember(std::string_view key, std::initializer_list<std::string_view> members)
   {
      push_smismember(key, members.begin(), members.end());
   }

   /**
    * @brief Appends an SMISMEMBER command to the end of the request.
    *
    * [`members_begin`, `members_end`) should point to a valid range of members.
    * If the range contains `["val1", "val2"]`, the resulting command is `SMISMEMBER key val1 val2`.
    *
    * @param key The set key.
    * @param members_begin Iterator to the beginning of the members.
    * @param members_end Iterator to the end of the members.
    */
   template <class ForwardIt>
   void push_smismember(std::string_view key, ForwardIt members_begin, ForwardIt members_end)
   {
      push_range("SMISMEMBER", key, members_begin, members_end);
   }

   /**
    * @brief Appends an SINTER command to the end of the request.
    *
    * If `keys` contains `{"set1", "set2"}`, the resulting command is `SINTER set1 set2`.
    *
    * @param keys The set keys to intersect.
    */
   void push_sinter(std::initializer_list<std::string_view> keys)
   {
      push_sinter(keys.begin(), keys.end());
   }

   /**
    * @brief Appends an SINTER command to the end of the request.
    *
    * [`keys_begin`, `keys_end`) should point to a valid range of set keys.
    * If the range contains `["set1", "set2"]`, the resulting command is `SINTER set1 set2`.
    *
    * @param keys_begin Iterator to the beginning of the keys.
    * @param keys_end Iterator to the end of the keys.
    */
   template <class ForwardIt>
   void push_sinter(ForwardIt keys_begin, ForwardIt keys_end)
   {
      push_range("SINTER", keys_begin, keys_end);
   }

   /**
    * @brief Appends an SINTERSTORE command to the end of the request.
    *
    * If `destination` is `"destset"` and `keys` contains `{"set1", "set2"}`, the resulting command is `SINTERSTORE destset set1 set2`.
    *
    * @param destination The destination key where the intersection will be stored.
    * @param keys The set keys to intersect.
    */
   void push_sinterstore(std::string_view destination, std::initializer_list<std::string_view> keys)
   {
      push_range("SINTERSTORE", destination, keys.begin(), keys.end());
   }

   /**
    * @brief Appends an SUNION command to the end of the request.
    *
    * If `keys` contains `{"set1", "set2"}`, the resulting command is `SUNION set1 set2`.
    *
    * @param keys The set keys to union.
    */
   void push_sunion(std::initializer_list<std::string_view> keys)
   {
      push_sunion(keys.begin(), keys.end());
   }

   /**
    * @brief Appends an SUNION command to the end of the request.
    *
    * [`keys_begin`, `keys_end`) should point to a valid range of set keys.
    * If the range contains `["set1", "set2"]`, the resulting command is `SUNION set1 set2`.
    *
    * @param keys_begin Iterator to the beginning of the keys.
    * @param keys_end Iterator to the end of the keys.
    */
   template <class ForwardIt>
   void push_sunion(ForwardIt keys_begin, ForwardIt keys_end)
   {
      push_range("SUNION", keys_begin, keys_end);
   }

   /**
    * @brief Appends an SUNIONSTORE command to the end of the request.
    *
    * If `destination` is `"destset"` and `keys` contains `{"set1", "set2"}`, the resulting command is `SUNIONSTORE destset set1 set2`.
    *
    * @param destination The destination key where the union will be stored.
    * @param keys The set keys to union.
    */
   void push_sunionstore(std::string_view destination, std::initializer_list<std::string_view> keys)
   {
      push_range("SUNIONSTORE", destination, keys.begin(), keys.end());
   }

   /**
    * @brief Appends an SDIFF command to the end of the request.
    *
    * If `keys` contains `{"set1", "set2"}`, the resulting command is `SDIFF set1 set2`.
    *
    * @param keys The set keys to compute the difference from.
    */
   void push_sdiff(std::initializer_list<std::string_view> keys)
   {
      push_sdiff(keys.begin(), keys.end());
   }

   /**
    * @brief Appends an SDIFF command to the end of the request.
    *
    * [`keys_begin`, `keys_end`) should point to a valid range of set keys.
    * If the range contains `["set1", "set2"]`, the resulting command is `SDIFF set1 set2`.
    *
    * @param keys_begin Iterator to the beginning of the keys.
    * @param keys_end Iterator to the end of the keys.
    */
   template <class ForwardIt>
   void push_sdiff(ForwardIt keys_begin, ForwardIt keys_end)
   {
      push_range("SDIFF", keys_begin, keys_end);
   }

   /**
    * @brief Appends an SDIFFSTORE command to the end of the request.
    *
    * If `destination` is `"destset"` and `keys` contains `{"set1", "set2"}`, the resulting command is `SDIFFSTORE destset set1 set2`.
    *
    * @param destination The destination key where the difference will be stored.
    * @param keys The set keys to compute the difference from.
    */
   void push_sdiffstore(std::string_view destination, std::initializer_list<std::string_view> keys)
   {
      push_range("SDIFFSTORE", destination, keys.begin(), keys.end());
   }

   /**
    * @brief Appends an SPOP command to the end of the request.
    *
    * If `key` is `"myset"`, the resulting command is `SPOP myset`.
    *
    * @param key The set key.
    */
   void push_spop(std::string_view key) { push("SPOP", key); }

   /**
    * @brief Appends an SPOP command to the end of the request.
    *
    * If `key` is `"myset"` and `count` is `2`, the resulting command is `SPOP myset 2`.
    *
    * @param key The set key.
    * @param count The number of random members to pop.
    */
   void push_spop(std::string_view key, int64_t count) { push("SPOP", key, count); }

   /**
    * @brief Appends an SRANDMEMBER command to the end of the request.
    *
    * If `key` is `"myset"`, the resulting command is `SRANDMEMBER myset`.
    *
    * @param key The set key.
    */
   void push_srandmember(std::string_view key) { push("SRANDMEMBER", key); }

   /**
    * @brief Appends an SRANDMEMBER command to the end of the request.
    *
    * If `key` is `"myset"` and `count` is `3`, the resulting command is `SRANDMEMBER myset 3`.
    *
    * @param key The set key.
    * @param count The number of random members to return.
    */
   void push_srandmember(std::string_view key, int64_t count) { push("SRANDMEMBER", key, count); }

   /**
    * @brief Appends an SMOVE command to the end of the request.
    *
    * If `source` is `"set1"`, `destination` is `"set2"`, and `member` is `"value"`, the resulting command is `SMOVE set1 set2 value`.
    *
    * @param source The source set key.
    * @param destination The destination set key.
    * @param member The member to move.
    */
   void push_smove(std::string_view source, std::string_view destination, std::string_view member)
   {
      push("SMOVE", source, destination, member);
   }

   /**
    * @brief Appends an SSCAN command to the end of the request.
    *
    * If `key` is `"myset"` and `cursor` is `0`, the resulting command is `SSCAN myset 0`.
    *
    * @param key The set key.
    * @param cursor The cursor position.
    */
   void push_sscan(std::string_view key, int64_t cursor) { push("SSCAN", key, cursor); }

   // ===== SORTED SET COMMANDS =====

   /**
    * @brief Appends a ZADD command to the end of the request.
    *
    * If `key` is `"myzset"`, `score` is `1.5`, and `member` is `"value1"`, the resulting command is `ZADD myzset 1.5 value1`.
    *
    * @param key The sorted set key.
    * @param score The score for the member.
    * @param member The member to add.
    */
   void push_zadd(std::string_view key, double score, std::string_view member);

   /**
    * @brief Appends a ZADD command to the end of the request.
    *
    * [`score_members_begin`, `score_members_end`) should point to a valid range of score-member pairs.
    * If the range contains `[(1.5, "val1"), (2.5, "val2")]`, the resulting command is `ZADD key 1.5 val1 2.5 val2`.
    *
    * @param key The sorted set key.
    * @param score_members_begin Iterator to the beginning of the score-member pairs.
    * @param score_members_end Iterator to the end of the score-member pairs.
    */
   template <class ForwardIt>
   void push_zadd(std::string_view key, ForwardIt score_members_begin, ForwardIt score_members_end)
   {
      push_range("ZADD", key, score_members_begin, score_members_end);
   }

   /**
    * @brief Appends a ZADD command to the end of the request.
    *
    * If `score_members` contains `[(1.5, "val1"), (2.5, "val2")]`, the resulting command is `ZADD key 1.5 val1 2.5 val2`.
    *
    * @param key The sorted set key.
    * @param score_members The score-member pairs to add.
    */
   template <class Range>
   void push_zadd(
      std::string_view key,
      Range&& score_members,
      decltype(std::cbegin(score_members))* = nullptr)
   {
      push_zadd(key, std::cbegin(score_members), std::cend(score_members));
   }

   /**
    * @brief Appends a ZREM command to the end of the request.
    *
    * If `key` is `"myzset"` and `members` contains `{"val1", "val2"}`, the resulting command is `ZREM myzset val1 val2`.
    *
    * @param key The sorted set key.
    * @param members The members to remove.
    */
   void push_zrem(std::string_view key, std::initializer_list<std::string_view> members)
   {
      push_zrem(key, members.begin(), members.end());
   }

   /**
    * @brief Appends a ZREM command to the end of the request.
    *
    * [`members_begin`, `members_end`) should point to a valid range of members.
    * If the range contains `["val1", "val2"]`, the resulting command is `ZREM key val1 val2`.
    *
    * @param key The sorted set key.
    * @param members_begin Iterator to the beginning of the members.
    * @param members_end Iterator to the end of the members.
    */
   template <class ForwardIt>
   void push_zrem(std::string_view key, ForwardIt members_begin, ForwardIt members_end)
   {
      push_range("ZREM", key, members_begin, members_end);
   }

   /**
    * @brief Appends a ZCARD command to the end of the request.
    *
    * If `key` is `"myzset"`, the resulting command is `ZCARD myzset`.
    *
    * @param key The sorted set key.
    */
   void push_zcard(std::string_view key) { push("ZCARD", key); }

   /**
    * @brief Appends a ZCOUNT command to the end of the request.
    *
    * If `key` is `"myzset"`, `min` is `1.0`, and `max` is `3.0`, the resulting command is `ZCOUNT myzset 1.0 3.0`.
    *
    * @param key The sorted set key.
    * @param min The minimum score (inclusive).
    * @param max The maximum score (inclusive).
    */
   void push_zcount(std::string_view key, double min, double max);

   /**
    * @brief Appends a ZSCORE command to the end of the request.
    *
    * If `key` is `"myzset"` and `member` is `"value1"`, the resulting command is `ZSCORE myzset value1`.
    *
    * @param key The sorted set key.
    * @param member The member whose score to retrieve.
    */
   void push_zscore(std::string_view key, std::string_view member) { push("ZSCORE", key, member); }

   /**
    * @brief Appends a ZMSCORE command to the end of the request.
    *
    * If `key` is `"myzset"` and `members` contains `{"val1", "val2"}`, the resulting command is `ZMSCORE myzset val1 val2`.
    *
    * @param key The sorted set key.
    * @param members The members whose scores to retrieve.
    */
   void push_zmscore(std::string_view key, std::initializer_list<std::string_view> members)
   {
      push_zmscore(key, members.begin(), members.end());
   }

   /**
    * @brief Appends a ZMSCORE command to the end of the request.
    *
    * [`members_begin`, `members_end`) should point to a valid range of members.
    * If the range contains `["val1", "val2"]`, the resulting command is `ZMSCORE key val1 val2`.
    *
    * @param key The sorted set key.
    * @param members_begin Iterator to the beginning of the members.
    * @param members_end Iterator to the end of the members.
    */
   template <class ForwardIt>
   void push_zmscore(std::string_view key, ForwardIt members_begin, ForwardIt members_end)
   {
      push_range("ZMSCORE", key, members_begin, members_end);
   }

   /**
    * @brief Appends a ZRANK command to the end of the request.
    *
    * If `key` is `"myzset"` and `member` is `"value1"`, the resulting command is `ZRANK myzset value1`.
    *
    * @param key The sorted set key.
    * @param member The member whose rank to retrieve.
    */
   void push_zrank(std::string_view key, std::string_view member) { push("ZRANK", key, member); }

   /**
    * @brief Appends a ZREVRANK command to the end of the request.
    *
    * If `key` is `"myzset"` and `member` is `"value1"`, the resulting command is `ZREVRANK myzset value1`.
    *
    * @param key The sorted set key.
    * @param member The member whose reverse rank to retrieve.
    */
   void push_zrevrank(std::string_view key, std::string_view member)
   {
      push("ZREVRANK", key, member);
   }

   /**
    * @brief Appends a ZRANGE command to the end of the request.
    *
    * If `key` is `"myzset"`, `start` is `0`, and `stop` is `10`, the resulting command is `ZRANGE myzset 0 10`.
    *
    * @param key The sorted set key.
    * @param start The start index.
    * @param stop The stop index.
    */
   void push_zrange(std::string_view key, int64_t start, int64_t stop)
   {
      push("ZRANGE", key, start, stop);
   }

   /**
    * @brief Appends a ZRANGE command to the end of the request.
    *
    * If `key` is `"myzset"`, `start` is `0`, `stop` is `10`, and `withscores` is `true`, the resulting command is `ZRANGE myzset 0 10 WITHSCORES`.
    *
    * @param key The sorted set key.
    * @param start The start index.
    * @param stop The stop index.
    * @param withscores If true, include scores in the response.
    */
   void push_zrange(std::string_view key, int64_t start, int64_t stop, bool withscores)
   {
      if (withscores)
         push("ZRANGE", key, start, stop, "WITHSCORES");
      else
         push("ZRANGE", key, start, stop);
   }

   /**
    * @brief Appends a ZREVRANGE command to the end of the request.
    *
    * If `key` is `"myzset"`, `start` is `0`, and `stop` is `10`, the resulting command is `ZREVRANGE myzset 0 10`.
    *
    * @param key The sorted set key.
    * @param start The start index.
    * @param stop The stop index.
    */
   void push_zrevrange(std::string_view key, int64_t start, int64_t stop)
   {
      push("ZREVRANGE", key, start, stop);
   }

   /**
    * @brief Appends a ZRANGEBYSCORE command to the end of the request.
    *
    * If `key` is `"myzset"`, `min` is `1.0`, and `max` is `5.0`, the resulting command is `ZRANGEBYSCORE myzset 1.0 5.0`.
    *
    * @param key The sorted set key.
    * @param min The minimum score.
    * @param max The maximum score.
    */
   void push_zrangebyscore(std::string_view key, double min, double max);

   /**
    * @brief Appends a ZREVRANGEBYSCORE command to the end of the request.
    *
    * If `key` is `"myzset"`, `max` is `5.0`, and `min` is `1.0`, the resulting command is `ZREVRANGEBYSCORE myzset 5.0 1.0`.
    *
    * @param key The sorted set key.
    * @param max The maximum score.
    * @param min The minimum score.
    */
   void push_zrevrangebyscore(std::string_view key, double max, double min);

   /**
    * @brief Appends a ZINCRBY command to the end of the request.
    *
    * If `key` is `"myzset"`, `increment` is `2.5`, and `member` is `"value1"`, the resulting command is `ZINCRBY myzset 2.5 value1`.
    *
    * @param key The sorted set key.
    * @param increment The amount to increment the score by.
    * @param member The member whose score to increment.
    */
   void push_zincrby(std::string_view key, double increment, std::string_view member);

   /**
    * @brief Appends a ZPOPMAX command to the end of the request.
    *
    * If `key` is `"myzset"`, the resulting command is `ZPOPMAX myzset`.
    *
    * @param key The sorted set key.
    */
   void push_zpopmax(std::string_view key) { push("ZPOPMAX", key); }

   /**
    * @brief Appends a ZPOPMAX command to the end of the request.
    *
    * If `key` is `"myzset"` and `count` is `3`, the resulting command is `ZPOPMAX myzset 3`.
    *
    * @param key The sorted set key.
    * @param count The number of members to pop.
    */
   void push_zpopmax(std::string_view key, int64_t count) { push("ZPOPMAX", key, count); }

   /**
    * @brief Appends a ZPOPMIN command to the end of the request.
    *
    * If `key` is `"myzset"`, the resulting command is `ZPOPMIN myzset`.
    *
    * @param key The sorted set key.
    */
   void push_zpopmin(std::string_view key) { push("ZPOPMIN", key); }

   /**
    * @brief Appends a ZPOPMIN command to the end of the request.
    *
    * If `key` is `"myzset"` and `count` is `3`, the resulting command is `ZPOPMIN myzset 3`.
    *
    * @param key The sorted set key.
    * @param count The number of members to pop.
    */
   void push_zpopmin(std::string_view key, int64_t count) { push("ZPOPMIN", key, count); }

   /**
    * @brief Appends a ZRANDMEMBER command to the end of the request.
    *
    * If `key` is `"myzset"`, the resulting command is `ZRANDMEMBER myzset`.
    *
    * @param key The sorted set key.
    */
   void push_zrandmember(std::string_view key) { push("ZRANDMEMBER", key); }

   /**
    * @brief Appends a ZRANDMEMBER command to the end of the request.
    *
    * If `key` is `"myzset"` and `count` is `3`, the resulting command is `ZRANDMEMBER myzset 3`.
    *
    * @param key The sorted set key.
    * @param count The number of random members to return.
    */
   void push_zrandmember(std::string_view key, int64_t count) { push("ZRANDMEMBER", key, count); }

   /**
    * @brief Appends a ZSCAN command to the end of the request.
    *
    * If `key` is `"myzset"` and `cursor` is `0`, the resulting command is `ZSCAN myzset 0`.
    *
    * @param key The sorted set key.
    * @param cursor The cursor position.
    */
   void push_zscan(std::string_view key, int64_t cursor) { push("ZSCAN", key, cursor); }

   // ===== KEY MANAGEMENT COMMANDS =====

   /**
    * @brief Appends a DEL command to the end of the request.
    *
    * If `key` is `"mykey"`, the resulting command is `DEL mykey`.
    *
    * @param key The key to delete.
    */
   void push_del(std::string_view key) { push("DEL", key); }

   /**
    * @brief Appends a DEL command to the end of the request.
    *
    * If `keys` contains `{"key1", "key2"}`, the resulting command is `DEL key1 key2`.
    *
    * @param keys The keys to delete.
    */
   void push_del(std::initializer_list<std::string_view> keys)
   {
      push_del(keys.begin(), keys.end());
   }

   /**
    * @brief Appends a DEL command to the end of the request.
    *
    * [`keys_begin`, `keys_end`) should point to a valid range of keys.
    * If the range contains `["key1", "key2"]`, the resulting command is `DEL key1 key2`.
    *
    * @param keys_begin Iterator to the beginning of the keys.
    * @param keys_end Iterator to the end of the keys.
    */
   template <class ForwardIt>
   void push_del(ForwardIt keys_begin, ForwardIt keys_end)
   {
      push_range("DEL", keys_begin, keys_end);
   }

   /**
    * @brief Appends an UNLINK command to the end of the request.
    *
    * If `key` is `"mykey"`, the resulting command is `UNLINK mykey`.
    * UNLINK is similar to DEL but performs the deletion asynchronously.
    *
    * @param key The key to unlink.
    */
   void push_unlink(std::string_view key) { push("UNLINK", key); }

   /**
    * @brief Appends an UNLINK command to the end of the request.
    *
    * If `keys` contains `{"key1", "key2"}`, the resulting command is `UNLINK key1 key2`.
    *
    * @param keys The keys to unlink.
    */
   void push_unlink(std::initializer_list<std::string_view> keys)
   {
      push_unlink(keys.begin(), keys.end());
   }

   /**
    * @brief Appends an UNLINK command to the end of the request.
    *
    * [`keys_begin`, `keys_end`) should point to a valid range of keys.
    * If the range contains `["key1", "key2"]`, the resulting command is `UNLINK key1 key2`.
    *
    * @param keys_begin Iterator to the beginning of the keys.
    * @param keys_end Iterator to the end of the keys.
    */
   template <class ForwardIt>
   void push_unlink(ForwardIt keys_begin, ForwardIt keys_end)
   {
      push_range("UNLINK", keys_begin, keys_end);
   }

   /**
    * @brief Appends an EXISTS command to the end of the request.
    *
    * If `key` is `"mykey"`, the resulting command is `EXISTS mykey`.
    *
    * @param key The key to check for existence.
    */
   void push_exists(std::string_view key) { push("EXISTS", key); }

   /**
    * @brief Appends an EXISTS command to the end of the request.
    *
    * If `keys` contains `{"key1", "key2"}`, the resulting command is `EXISTS key1 key2`.
    *
    * @param keys The keys to check for existence.
    */
   void push_exists(std::initializer_list<std::string_view> keys)
   {
      push_exists(keys.begin(), keys.end());
   }

   /**
    * @brief Appends an EXISTS command to the end of the request.
    *
    * [`keys_begin`, `keys_end`) should point to a valid range of keys.
    * If the range contains `["key1", "key2"]`, the resulting command is `EXISTS key1 key2`.
    *
    * @param keys_begin Iterator to the beginning of the keys.
    * @param keys_end Iterator to the end of the keys.
    */
   template <class ForwardIt>
   void push_exists(ForwardIt keys_begin, ForwardIt keys_end)
   {
      push_range("EXISTS", keys_begin, keys_end);
   }

   /**
    * @brief Appends an EXPIRE command to the end of the request.
    *
    * If `key` is `"mykey"` and `seconds` is `60`, the resulting command is `EXPIRE mykey 60`.
    *
    * @param key The key to set expiration on.
    * @param seconds The expiration time in seconds.
    */
   void push_expire(std::string_view key, std::chrono::seconds seconds)
   {
      push("EXPIRE", key, seconds.count());
   }

   /**
    * @brief Appends an EXPIREAT command to the end of the request.
    *
    * If `key` is `"mykey"` and `tp` is a time point, the resulting command is `EXPIREAT mykey <timestamp>`.
    *
    * @param key The key to set expiration on.
    * @param tp The absolute expiration time point.
    */
   void push_expireat(std::string_view key, std::chrono::system_clock::time_point tp)
   {
      auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch())
                          .count();
      push("EXPIREAT", key, timestamp);
   }

   /**
    * @brief Appends a PEXPIRE command to the end of the request.
    *
    * If `key` is `"mykey"` and `ms` is `5000`, the resulting command is `PEXPIRE mykey 5000`.
    *
    * @param key The key to set expiration on.
    * @param ms The expiration time in milliseconds.
    */
   void push_pexpire(std::string_view key, std::chrono::milliseconds ms)
   {
      push("PEXPIRE", key, ms.count());
   }

   /**
    * @brief Appends a PEXPIREAT command to the end of the request.
    *
    * If `key` is `"mykey"` and `tp` is a time point, the resulting command is `PEXPIREAT mykey <timestamp>`.
    *
    * @param key The key to set expiration on.
    * @param tp The absolute expiration time point.
    */
   void push_pexpireat(std::string_view key, std::chrono::system_clock::time_point tp)
   {
      auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch())
                          .count();
      push("PEXPIREAT", key, timestamp);
   }

   /**
    * @brief Appends a TTL command to the end of the request.
    *
    * If `key` is `"mykey"`, the resulting command is `TTL mykey`.
    *
    * @param key The key to get the time-to-live for.
    */
   void push_ttl(std::string_view key) { push("TTL", key); }

   /**
    * @brief Appends a PTTL command to the end of the request.
    *
    * If `key` is `"mykey"`, the resulting command is `PTTL mykey`.
    *
    * @param key The key to get the time-to-live in milliseconds for.
    */
   void push_pttl(std::string_view key) { push("PTTL", key); }

   /**
    * @brief Appends a PERSIST command to the end of the request.
    *
    * If `key` is `"mykey"`, the resulting command is `PERSIST mykey`.
    *
    * @param key The key to remove the expiration from.
    */
   void push_persist(std::string_view key) { push("PERSIST", key); }

   /**
    * @brief Appends a KEYS command to the end of the request.
    *
    * If `pattern` is `"user:*"`, the resulting command is `KEYS user:*`.
    *
    * @param pattern The pattern to match keys against.
    */
   void push_keys(std::string_view pattern) { push("KEYS", pattern); }

   /**
    * @brief Appends a RANDOMKEY command to the end of the request.
    *
    * The resulting command is `RANDOMKEY`.
    */
   void push_randomkey() { push("RANDOMKEY"); }

   /**
    * @brief Appends a RENAME command to the end of the request.
    *
    * If `key` is `"oldkey"` and `newkey` is `"newkey"`, the resulting command is `RENAME oldkey newkey`.
    *
    * @param key The current key name.
    * @param newkey The new key name.
    */
   void push_rename(std::string_view key, std::string_view newkey) { push("RENAME", key, newkey); }

   /**
    * @brief Appends a RENAMENX command to the end of the request.
    *
    * If `key` is `"oldkey"` and `newkey` is `"newkey"`, the resulting command is `RENAMENX oldkey newkey`.
    * RENAMENX only renames if the new key does not exist.
    *
    * @param key The current key name.
    * @param newkey The new key name.
    */
   void push_renamenx(std::string_view key, std::string_view newkey)
   {
      push("RENAMENX", key, newkey);
   }

   /**
    * @brief Appends a TYPE command to the end of the request.
    *
    * If `key` is `"mykey"`, the resulting command is `TYPE mykey`.
    *
    * @param key The key to get the type of.
    */
   void push_type(std::string_view key) { push("TYPE", key); }

   /**
    * @brief Appends a SCAN command to the end of the request.
    *
    * If `cursor` is `0`, the resulting command is `SCAN 0`.
    *
    * @param cursor The cursor position.
    */
   void push_scan(int64_t cursor) { push("SCAN", cursor); }

   /**
    * @brief Appends a SCAN command to the end of the request.
    *
    * If `cursor` is `0`, `pattern` is `"user:*"`, and `count` is `100`, the resulting command is `SCAN 0 MATCH user:* COUNT 100`.
    *
    * @param cursor The cursor position.
    * @param pattern The pattern to match keys against.
    * @param count The number of elements to return per iteration.
    */
   void push_scan(int64_t cursor, std::string_view pattern, int64_t count)
   {
      push("SCAN", cursor, "MATCH", pattern, "COUNT", count);
   }

   /**
    * @brief Appends a COPY command to the end of the request.
    *
    * If `source` is `"sourcekey"` and `destination` is `"destkey"`, the resulting command is `COPY sourcekey destkey`.
    *
    * @param source The source key.
    * @param destination The destination key.
    */
   void push_copy(std::string_view source, std::string_view destination)
   {
      push("COPY", source, destination);
   }

   /**
    * @brief Appends a COPY command to the end of the request.
    *
    * If `source` is `"sourcekey"`, `destination` is `"destkey"`, and `replace` is `true`, the resulting command is `COPY sourcekey destkey REPLACE`.
    *
    * @param source The source key.
    * @param destination The destination key.
    * @param replace If true, replace the destination key if it exists.
    */
   void push_copy(std::string_view source, std::string_view destination, bool replace)
   {
      if (replace)
         push("COPY", source, destination, "REPLACE");
      else
         push("COPY", source, destination);
   }

   /**
    * @brief Appends a MOVE command to the end of the request.
    *
    * If `key` is `"mykey"` and `db` is `1`, the resulting command is `MOVE mykey 1`.
    *
    * @param key The key to move.
    * @param db The target database index.
    */
   void push_move(std::string_view key, int64_t db) { push("MOVE", key, db); }

   /**
    * @brief Appends a TOUCH command to the end of the request.
    *
    * If `keys` contains `{"key1", "key2"}`, the resulting command is `TOUCH key1 key2`.
    *
    * @param keys The keys to touch.
    */
   void push_touch(std::initializer_list<std::string_view> keys)
   {
      push_touch(keys.begin(), keys.end());
   }

   /**
    * @brief Appends a TOUCH command to the end of the request.
    *
    * [`keys_begin`, `keys_end`) should point to a valid range of keys.
    * If the range contains `["key1", "key2"]`, the resulting command is `TOUCH key1 key2`.
    *
    * @param keys_begin Iterator to the beginning of the keys.
    * @param keys_end Iterator to the end of the keys.
    */
   template <class ForwardIt>
   void push_touch(ForwardIt keys_begin, ForwardIt keys_end)
   {
      push_range("TOUCH", keys_begin, keys_end);
   }

   /**
    * @brief Appends a DUMP command to the end of the request.
    *
    * If `key` is `"mykey"`, the resulting command is `DUMP mykey`.
    *
    * @param key The key to serialize.
    */
   void push_dump(std::string_view key) { push("DUMP", key); }

   /**
    * @brief Appends a RESTORE command to the end of the request.
    *
    * If `key` is `"mykey"`, `ttl` is `0`, and `serialized_value` is a serialized value, the resulting command is `RESTORE mykey 0 <value>`.
    *
    * @param key The key to restore.
    * @param ttl The time-to-live in milliseconds (0 for no expiration).
    * @param serialized_value The serialized value from DUMP.
    */
   template <class T>
   void push_restore(std::string_view key, std::chrono::milliseconds ttl, const T& serialized_value)
   {
      push("RESTORE", key, ttl.count(), serialized_value);
   }

   // ===== BITMAP COMMANDS =====

   /**
    * @brief Appends a SETBIT command to the end of the request.
    *
    * If `key` is `"mybitmap"`, `offset` is `7`, and `value` is `1`, the resulting command is `SETBIT mybitmap 7 1`.
    *
    * @param key The key storing the bitmap.
    * @param offset The bit offset.
    * @param value The bit value (0 or 1).
    */
   void push_setbit(std::string_view key, int64_t offset, int value)
   {
      push("SETBIT", key, offset, value);
   }

   /**
    * @brief Appends a GETBIT command to the end of the request.
    *
    * If `key` is `"mybitmap"` and `offset` is `7`, the resulting command is `GETBIT mybitmap 7`.
    *
    * @param key The key storing the bitmap.
    * @param offset The bit offset.
    */
   void push_getbit(std::string_view key, int64_t offset) { push("GETBIT", key, offset); }

   /**
    * @brief Appends a BITCOUNT command to the end of the request.
    *
    * If `key` is `"mybitmap"`, the resulting command is `BITCOUNT mybitmap`.
    *
    * @param key The key storing the bitmap.
    */
   void push_bitcount(std::string_view key) { push("BITCOUNT", key); }

   /**
    * @brief Appends a BITCOUNT command to the end of the request.
    *
    * If `key` is `"mybitmap"`, `start` is `0`, and `end` is `1`, the resulting command is `BITCOUNT mybitmap 0 1`.
    *
    * @param key The key storing the bitmap.
    * @param start The start byte offset.
    * @param end The end byte offset.
    */
   void push_bitcount(std::string_view key, int64_t start, int64_t end)
   {
      push("BITCOUNT", key, start, end);
   }

   /**
    * @brief Appends a BITPOS command to the end of the request.
    *
    * If `key` is `"mybitmap"` and `bit` is `1`, the resulting command is `BITPOS mybitmap 1`.
    *
    * @param key The key storing the bitmap.
    * @param bit The bit value to search for (0 or 1).
    */
   void push_bitpos(std::string_view key, int bit) { push("BITPOS", key, bit); }

   /**
    * @brief Appends a BITPOS command to the end of the request.
    *
    * If `key` is `"mybitmap"`, `bit` is `1`, `start` is `0`, and `end` is `2`, the resulting command is `BITPOS mybitmap 1 0 2`.
    *
    * @param key The key storing the bitmap.
    * @param bit The bit value to search for (0 or 1).
    * @param start The start byte offset.
    * @param end The end byte offset.
    */
   void push_bitpos(std::string_view key, int bit, int64_t start, int64_t end)
   {
      push("BITPOS", key, bit, start, end);
   }

   /**
    * @brief Appends a BITOP AND command to the end of the request.
    *
    * If `destkey` is `"dest"` and `keys` contains `{"key1", "key2"}`, the resulting command is `BITOP AND dest key1 key2`.
    *
    * @param destkey The destination key for the result.
    * @param keys The source keys to perform AND operation on.
    */
   void push_bitop_and(std::string_view destkey, std::initializer_list<std::string_view> keys);

   /**
    * @brief Appends a BITOP OR command to the end of the request.
    *
    * If `destkey` is `"dest"` and `keys` contains `{"key1", "key2"}`, the resulting command is `BITOP OR dest key1 key2`.
    *
    * @param destkey The destination key for the result.
    * @param keys The source keys to perform OR operation on.
    */
   void push_bitop_or(std::string_view destkey, std::initializer_list<std::string_view> keys);

   /**
    * @brief Appends a BITOP XOR command to the end of the request.
    *
    * If `destkey` is `"dest"` and `keys` contains `{"key1", "key2"}`, the resulting command is `BITOP XOR dest key1 key2`.
    *
    * @param destkey The destination key for the result.
    * @param keys The source keys to perform XOR operation on.
    */
   void push_bitop_xor(std::string_view destkey, std::initializer_list<std::string_view> keys);

   /**
    * @brief Appends a BITOP NOT command to the end of the request.
    *
    * If `destkey` is `"dest"` and `key` is `"source"`, the resulting command is `BITOP NOT dest source`.
    *
    * @param destkey The destination key for the result.
    * @param key The source key to perform NOT operation on.
    */
   void push_bitop_not(std::string_view destkey, std::string_view key)
   {
      push("BITOP", "NOT", destkey, key);
   }

   // ===== HYPERLOGLOG COMMANDS =====

   /**
    * @brief Appends a PFADD command to the end of the request.
    *
    * If `key` is `"hll"` and `elements` contains `{"val1", "val2"}`, the resulting command is `PFADD hll val1 val2`.
    *
    * @param key The HyperLogLog key.
    * @param elements The elements to add.
    */
   void push_pfadd(std::string_view key, std::initializer_list<std::string_view> elements)
   {
      push_pfadd(key, elements.begin(), elements.end());
   }

   /**
    * @brief Appends a PFADD command to the end of the request.
    *
    * [`elements_begin`, `elements_end`) should point to a valid range of elements.
    * If the range contains `["val1", "val2"]`, the resulting command is `PFADD key val1 val2`.
    *
    * @param key The HyperLogLog key.
    * @param elements_begin Iterator to the beginning of the elements.
    * @param elements_end Iterator to the end of the elements.
    */
   template <class ForwardIt>
   void push_pfadd(std::string_view key, ForwardIt elements_begin, ForwardIt elements_end)
   {
      push_range("PFADD", key, elements_begin, elements_end);
   }

   /**
    * @brief Appends a PFCOUNT command to the end of the request.
    *
    * If `key` is `"hll"`, the resulting command is `PFCOUNT hll`.
    *
    * @param key The HyperLogLog key.
    */
   void push_pfcount(std::string_view key) { push("PFCOUNT", key); }

   /**
    * @brief Appends a PFCOUNT command to the end of the request.
    *
    * If `keys` contains `{"hll1", "hll2"}`, the resulting command is `PFCOUNT hll1 hll2`.
    *
    * @param keys The HyperLogLog keys.
    */
   void push_pfcount(std::initializer_list<std::string_view> keys)
   {
      push_pfcount(keys.begin(), keys.end());
   }

   /**
    * @brief Appends a PFCOUNT command to the end of the request.
    *
    * [`keys_begin`, `keys_end`) should point to a valid range of keys.
    * If the range contains `["hll1", "hll2"]`, the resulting command is `PFCOUNT hll1 hll2`.
    *
    * @param keys_begin Iterator to the beginning of the keys.
    * @param keys_end Iterator to the end of the keys.
    */
   template <class ForwardIt>
   void push_pfcount(ForwardIt keys_begin, ForwardIt keys_end)
   {
      push_range("PFCOUNT", keys_begin, keys_end);
   }

   /**
    * @brief Appends a PFMERGE command to the end of the request.
    *
    * If `destkey` is `"dest"` and `sourcekeys` contains `{"hll1", "hll2"}`, the resulting command is `PFMERGE dest hll1 hll2`.
    *
    * @param destkey The destination HyperLogLog key.
    * @param sourcekeys The source HyperLogLog keys to merge.
    */
   void push_pfmerge(std::string_view destkey, std::initializer_list<std::string_view> sourcekeys)
   {
      push_range("PFMERGE", destkey, sourcekeys.begin(), sourcekeys.end());
   }

   // ===== GEOSPATIAL COMMANDS =====

   /**
    * @brief Appends a GEOADD command to the end of the request.
    *
    * If `key` is `"locations"`, `longitude` is `13.361389`, `latitude` is `38.115556`, and `member` is `"Palermo"`,
    * the resulting command is `GEOADD locations 13.361389 38.115556 Palermo`.
    *
    * @param key The key of the geospatial index.
    * @param longitude The longitude coordinate (-180 to 180).
    * @param latitude The latitude coordinate (-85.05112878 to 85.05112878).
    * @param member The name of the member to add.
    */
   void push_geoadd(
      std::string_view key,
      double longitude,
      double latitude,
      std::string_view member);

   /**
    * @brief Appends a GEOADD command to the end of the request.
    *
    * Adds multiple longitude/latitude/member triples to the geospatial index.
    * The iterator range should contain alternating longitude, latitude, and member values.
    *
    * @param key The key of the geospatial index.
    * @param coord_members_begin Iterator to the beginning of the longitude/latitude/member sequence.
    * @param coord_members_end Iterator to the end of the longitude/latitude/member sequence.
    */
   template <class ForwardIt>
   void push_geoadd(
      std::string_view key,
      ForwardIt coord_members_begin,
      ForwardIt coord_members_end)
   {
      push_range("GEOADD", key, coord_members_begin, coord_members_end);
   }

   /**
    * @brief Appends a GEODIST command to the end of the request.
    *
    * If `key` is `"locations"`, `member1` is `"Palermo"`, and `member2` is `"Catania"`,
    * the resulting command is `GEODIST locations Palermo Catania`. Returns the distance in meters by default.
    *
    * @param key The key of the geospatial index.
    * @param member1 The first member.
    * @param member2 The second member.
    */
   void push_geodist(std::string_view key, std::string_view member1, std::string_view member2)
   {
      push("GEODIST", key, member1, member2);
   }

   /**
    * @brief Appends a GEODIST command to the end of the request.
    *
    * If `key` is `"locations"`, `member1` is `"Palermo"`, `member2` is `"Catania"`, and `unit` is `"km"`,
    * the resulting command is `GEODIST locations Palermo Catania km`. Returns the distance in the specified unit.
    *
    * @param key The key of the geospatial index.
    * @param member1 The first member.
    * @param member2 The second member.
    * @param unit The unit of distance: "m" (meters), "km" (kilometers), "mi" (miles), or "ft" (feet).
    */
   void push_geodist(
      std::string_view key,
      std::string_view member1,
      std::string_view member2,
      std::string_view unit)
   {
      push("GEODIST", key, member1, member2, unit);
   }

   /**
    * @brief Appends a GEOHASH command to the end of the request.
    *
    * If `key` is `"locations"` and `members` is `{"Palermo", "Catania"}`,
    * the resulting command is `GEOHASH locations Palermo Catania`. Returns geohash strings for the members.
    *
    * @param key The key of the geospatial index.
    * @param members Initializer list of member names.
    */
   void push_geohash(std::string_view key, std::initializer_list<std::string_view> members)
   {
      push_geohash(key, members.begin(), members.end());
   }

   /**
    * @brief Appends a GEOHASH command to the end of the request.
    *
    * Returns geohash strings for one or more members in a geospatial index.
    *
    * @param key The key of the geospatial index.
    * @param members_begin Iterator to the beginning of the member names.
    * @param members_end Iterator to the end of the member names.
    */
   template <class ForwardIt>
   void push_geohash(std::string_view key, ForwardIt members_begin, ForwardIt members_end)
   {
      push_range("GEOHASH", key, members_begin, members_end);
   }

   /**
    * @brief Appends a GEOPOS command to the end of the request.
    *
    * If `key` is `"locations"` and `members` is `{"Palermo", "Catania"}`,
    * the resulting command is `GEOPOS locations Palermo Catania`. Returns longitude/latitude coordinates for the members.
    *
    * @param key The key of the geospatial index.
    * @param members Initializer list of member names.
    */
   void push_geopos(std::string_view key, std::initializer_list<std::string_view> members)
   {
      push_geopos(key, members.begin(), members.end());
   }

   /**
    * @brief Appends a GEOPOS command to the end of the request.
    *
    * Returns longitude/latitude coordinates for one or more members in a geospatial index.
    *
    * @param key The key of the geospatial index.
    * @param members_begin Iterator to the beginning of the member names.
    * @param members_end Iterator to the end of the member names.
    */
   template <class ForwardIt>
   void push_geopos(std::string_view key, ForwardIt members_begin, ForwardIt members_end)
   {
      push_range("GEOPOS", key, members_begin, members_end);
   }

   /**
    * @brief Appends a GEOSEARCH command to the end of the request.
    *
    * If `key` is `"locations"`, `longitude` is `15`, `latitude` is `37`, `radius` is `200`, and `unit` is `"km"`,
    * the resulting command is `GEOSEARCH locations FROMLONLAT 15 37 BYRADIUS 200 km`.
    * Searches for members within the specified radius from the given coordinates.
    *
    * @param key The key of the geospatial index.
    * @param longitude The longitude coordinate of the center point.
    * @param latitude The latitude coordinate of the center point.
    * @param radius The radius of the search area.
    * @param unit The unit of the radius: "m" (meters), "km" (kilometers), "mi" (miles), or "ft" (feet).
    */
   void push_geosearch(
      std::string_view key,
      double longitude,
      double latitude,
      double radius,
      std::string_view unit);

   // ===== STREAM COMMANDS =====

   /**
    * @brief Appends an XADD command to the end of the request.
    *
    * If `key` is `"mystream"`, `id` is `"*"` (auto-generate), and the field-value pairs are `{"temp", "25", "humidity", "60"}`,
    * the resulting command is `XADD mystream * temp 25 humidity 60`. Adds an entry to a stream.
    *
    * @param key The key of the stream.
    * @param id The entry ID (use "*" to auto-generate based on timestamp).
    * @param fields_begin Iterator to the beginning of field-value pairs.
    * @param fields_end Iterator to the end of field-value pairs.
    */
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

   /**
    * @brief Appends an XLEN command to the end of the request.
    *
    * If `key` is `"mystream"`, the resulting command is `XLEN mystream`. Returns the number of entries in the stream.
    *
    * @param key The key of the stream.
    */
   void push_xlen(std::string_view key) { push("XLEN", key); }

   /**
    * @brief Appends an XRANGE command to the end of the request.
    *
    * If `key` is `"mystream"`, `start` is `"-"` (minimum ID), and `end` is `"+"` (maximum ID),
    * the resulting command is `XRANGE mystream - +`. Returns entries in a stream within a range of IDs.
    *
    * @param key The key of the stream.
    * @param start The minimum ID (use "-" for the smallest ID in the stream).
    * @param end The maximum ID (use "+" for the greatest ID in the stream).
    */
   void push_xrange(std::string_view key, std::string_view start, std::string_view end)
   {
      push("XRANGE", key, start, end);
   }

   /**
    * @brief Appends an XREVRANGE command to the end of the request.
    *
    * If `key` is `"mystream"`, `end` is `"+"` (maximum ID), and `start` is `"-"` (minimum ID),
    * the resulting command is `XREVRANGE mystream + -`. Returns entries in reverse order within a range of IDs.
    *
    * @param key The key of the stream.
    * @param end The maximum ID (use "+" for the greatest ID in the stream).
    * @param start The minimum ID (use "-" for the smallest ID in the stream).
    */
   void push_xrevrange(std::string_view key, std::string_view end, std::string_view start)
   {
      push("XREVRANGE", key, end, start);
   }

   /**
    * @brief Appends an XDEL command to the end of the request.
    *
    * If `key` is `"mystream"` and `ids` is `{"1526569495631-0", "1526569498055-0"}`,
    * the resulting command is `XDEL mystream 1526569495631-0 1526569498055-0`. Deletes entries from the stream.
    *
    * @param key The key of the stream.
    * @param ids Initializer list of entry IDs to delete.
    */
   void push_xdel(std::string_view key, std::initializer_list<std::string_view> ids)
   {
      push_xdel(key, ids.begin(), ids.end());
   }

   /**
    * @brief Appends an XDEL command to the end of the request.
    *
    * Deletes one or more entries from a stream by their IDs.
    *
    * @param key The key of the stream.
    * @param ids_begin Iterator to the beginning of entry IDs to delete.
    * @param ids_end Iterator to the end of entry IDs to delete.
    */
   template <class ForwardIt>
   void push_xdel(std::string_view key, ForwardIt ids_begin, ForwardIt ids_end)
   {
      push_range("XDEL", key, ids_begin, ids_end);
   }

   /**
    * @brief Appends an XTRIM command to the end of the request.
    *
    * If `key` is `"mystream"` and `maxlen` is `1000`,
    * the resulting command is `XTRIM mystream MAXLEN 1000`. Trims the stream to a maximum length.
    *
    * @param key The key of the stream.
    * @param maxlen The maximum number of entries to retain in the stream.
    */
   void push_xtrim(std::string_view key, int64_t maxlen) { push("XTRIM", key, "MAXLEN", maxlen); }

   /**
    * @brief Appends an XREAD command to the end of the request.
    *
    * If `count` is `10`, `key` is `"mystream"`, and `id` is `"0"` (read from beginning),
    * the resulting command is `XREAD COUNT 10 STREAMS mystream 0`. Reads entries from a stream.
    *
    * @param count The maximum number of entries to read.
    * @param key The key of the stream.
    * @param id The ID to read from (use "0" to read from the beginning, "$" for new entries only).
    */
   void push_xread(int64_t count, std::string_view key, std::string_view id)
   {
      push("XREAD", "COUNT", count, "STREAMS", key, id);
   }

   // ===== JSON COMMANDS (RedisJSON module) =====

   /**
    * @brief Appends a JSON.SET command to the end of the request.
    *
    * If `key` is `"user:1"`, `path` is `"$"` (root), and `json` is `"{\"name\":\"John\",\"age\":30}"`,
    * the resulting command is `JSON.SET user:1 $ {"name":"John","age":30}`. Sets a JSON value at the specified path.
    * Requires the RedisJSON module.
    *
    * @param key The key where the JSON document is stored.
    * @param path The JSONPath to the value (use "$" for root, "$.field" for nested fields).
    * @param json The JSON value to set.
    */
   template <class T>
   void push_json_set(std::string_view key, std::string_view path, const T& json)
   {
      push("JSON.SET", key, path, json);
   }

   /**
    * @brief Appends a JSON.GET command to the end of the request.
    *
    * If `key` is `"user:1"`, the resulting command is `JSON.GET user:1`. Gets the entire JSON document at the root path.
    * Requires the RedisJSON module.
    *
    * @param key The key where the JSON document is stored.
    */
   void push_json_get(std::string_view key) { push("JSON.GET", key); }

   /**
    * @brief Appends a JSON.GET command to the end of the request.
    *
    * If `key` is `"user:1"` and `path` is `"$.name"`, the resulting command is `JSON.GET user:1 $.name`.
    * Gets the JSON value at the specified path. Requires the RedisJSON module.
    *
    * @param key The key where the JSON document is stored.
    * @param path The JSONPath to the value (e.g., "$" for root, "$.field" for nested fields).
    */
   void push_json_get(std::string_view key, std::string_view path) { push("JSON.GET", key, path); }

   /**
    * @brief Appends a JSON.DEL command to the end of the request.
    *
    * If `key` is `"user:1"`, the resulting command is `JSON.DEL user:1`. Deletes the entire JSON document.
    * Requires the RedisJSON module.
    *
    * @param key The key where the JSON document is stored.
    */
   void push_json_del(std::string_view key) { push("JSON.DEL", key); }

   /**
    * @brief Appends a JSON.DEL command to the end of the request.
    *
    * If `key` is `"user:1"` and `path` is `"$.age"`, the resulting command is `JSON.DEL user:1 $.age`.
    * Deletes the JSON value at the specified path. Requires the RedisJSON module.
    *
    * @param key The key where the JSON document is stored.
    * @param path The JSONPath to the value to delete.
    */
   void push_json_del(std::string_view key, std::string_view path) { push("JSON.DEL", key, path); }

   /**
    * @brief Appends a JSON.MGET command to the end of the request.
    *
    * If `keys` is `{"user:1", "user:2", "user:3"}` and `path` is `"$.name"`,
    * the resulting command is `JSON.MGET user:1 user:2 user:3 $.name`. Gets JSON values from multiple keys at the specified path.
    * Requires the RedisJSON module.
    *
    * @param keys Initializer list of keys to retrieve JSON values from.
    * @param path The JSONPath to the value in each document.
    */
   void push_json_mget(std::initializer_list<std::string_view> keys, std::string_view path)
   {
      push_range("JSON.MGET", keys.begin(), keys.end());
      push("JSON.MGET", path);
   }

   /**
    * @brief Appends a JSON.TYPE command to the end of the request.
    *
    * If `key` is `"user:1"`, the resulting command is `JSON.TYPE user:1`. Returns the type of the JSON value at the root path.
    * Requires the RedisJSON module.
    *
    * @param key The key where the JSON document is stored.
    */
   void push_json_type(std::string_view key) { push("JSON.TYPE", key); }

   /**
    * @brief Appends a JSON.TYPE command to the end of the request.
    *
    * If `key` is `"user:1"` and `path` is `"$.age"`, the resulting command is `JSON.TYPE user:1 $.age`.
    * Returns the type of the JSON value at the specified path (e.g., "string", "number", "object", "array", "boolean", "null").
    * Requires the RedisJSON module.
    *
    * @param key The key where the JSON document is stored.
    * @param path The JSONPath to the value.
    */
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
