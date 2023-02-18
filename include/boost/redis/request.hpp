/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_REQUEST_HPP
#define BOOST_REDIS_REQUEST_HPP

#include <boost/redis/resp3/type.hpp>
#include <boost/redis/resp3/serialization.hpp>

#include <string>
#include <tuple>

// NOTE: For some commands like hset it would be a good idea to assert
// the value type is a pair.

namespace boost::redis {

namespace detail{
auto has_response(std::string_view cmd) -> bool;
}

/** \brief Creates Redis requests.
 *  \ingroup high-level-api
 *  
 *  A request is composed of one or more Redis commands and is
 *  referred to in the redis documentation as a pipeline, see
 *  https://redis.io/topics/pipelining. For example
 *
 *  @code
 *  request r;
 *  r.push("HELLO", 3);
 *  r.push("FLUSHALL");
 *  r.push("PING");
 *  r.push("PING", "key");
 *  r.push("QUIT");
 *  @endcode
 *
 *  \remarks
 *
 *  \li Non-string types will be converted to string by using \c
 *  boost_redis_to_bulk, which must be made available over ADL.
 *  \li Uses a std::string for internal storage.
 */
class request {
public:
   /// Request configuration options.
   struct config {
      /** \brief If `true` 
       * `boost::redis::connection::async_exec` will complete with error if the
       * connection is lost. Affects only requests that haven't been
       * sent yet.
       */
      bool cancel_on_connection_lost = true;

      /** \brief If `true` the request will complete with
       * boost::redis::error::not_connected if `async_exec` is called before
       * the connection with Redis was established.
       */
      bool cancel_if_not_connected = false;

      /** \brief If `false` `boost::redis::connection::async_exec` will not
       * automatically cancel this request if the connection is lost.
       * Affects only requests that have been written to the socket
       * but remained unresponded when `boost::redis::connection::async_run`
       * completed.
       */
      bool cancel_if_unresponded = true;

      /** \brief If this request has a `HELLO` command and this flag is
       * `true`, the `boost::redis::connection` will move it to the front of
       * the queue of awaiting requests. This makes it possible to
       * send `HELLO` and authenticate before other commands are sent.
       */
      bool hello_with_priority = true;
   };

   /** \brief Constructor
    *  
    *  \param cfg Configuration options.
    */
    explicit
    request(config cfg = config{true, false, true, true})
    : cfg_{cfg} {}

    //// Returns the number of commands contained in this request.
   [[nodiscard]] auto size() const noexcept -> std::size_t
      { return commands_;};

   [[nodiscard]] auto payload() const noexcept -> auto const&
      { return payload_;}

   [[nodiscard]] auto has_hello_priority() const noexcept -> auto const&
      { return has_hello_priority_;}

   /// Clears the request preserving allocated memory.
   void clear()
   {
      payload_.clear();
      commands_ = 0;
   }

   /// Calls std::string::reserve on the internal storage.
   void reserve(std::size_t new_cap = 0)
      { payload_.reserve(new_cap); }

   /// Returns a const reference to the config object.
   [[nodiscard]] auto get_config() const noexcept -> auto const& {return cfg_; }

   /// Returns a reference to the config object.
   [[nodiscard]] auto get_config() noexcept -> auto& {return cfg_; }

   /** @brief Appends a new command to the end of the request.
    *
    *  For example
    *
    *  \code
    *  request req;
    *  req.push("SET", "key", "some string", "EX", "2");
    *  \endcode
    *
    *  will add the \c set command with value "some string" and an
    *  expiration of 2 seconds.
    *
    *  \param cmd The command e.g redis or sentinel command.
    *  \param args Command arguments.
    */
   template <class... Ts>
   void push(std::string_view cmd, Ts const&... args)
   {
      using resp3::type;

      auto constexpr pack_size = sizeof...(Ts);
      resp3::add_header(payload_, type::array, 1 + pack_size);
      resp3::add_bulk(payload_, cmd);
      resp3::add_bulk(payload_, std::tie(std::forward<Ts const&>(args)...));

      check_cmd(cmd);
   }

   /** @brief Appends a new command to the end of the request.
    *  
    *  This overload is useful for commands that have a key and have a
    *  dynamic range of arguments. For example
    *
    *  @code
    *  std::map<std::string, std::string> map
    *     { {"key1", "value1"}
    *     , {"key2", "value2"}
    *     , {"key3", "value3"}
    *     };
    *
    *  request req;
    *  req.push_range("HSET", "key", std::cbegin(map), std::cend(map));
    *  @endcode
    *  
    *  \param cmd The command e.g. Redis or Sentinel command.
    *  \param key The command key.
    *  \param begin Iterator to the begin of the range.
    *  \param end Iterator to the end of the range.
    */
   template <class Key, class ForwardIterator>
   void push_range(std::string_view cmd, Key const& key, ForwardIterator begin, ForwardIterator end,
                    typename std::iterator_traits<ForwardIterator>::value_type * = nullptr)
   {
      using value_type = typename std::iterator_traits<ForwardIterator>::value_type;
      using resp3::type;

      if (begin == end)
         return;

      auto constexpr size = resp3::bulk_counter<value_type>::size;
      auto const distance = std::distance(begin, end);
      resp3::add_header(payload_, type::array, 2 + size * distance);
      resp3::add_bulk(payload_, cmd);
      resp3::add_bulk(payload_, key);

      for (; begin != end; ++begin)
	 resp3::add_bulk(payload_, *begin);

      check_cmd(cmd);
   }

   /** @brief Appends a new command to the end of the request.
    *
    *  This overload is useful for commands that have a dynamic number
    *  of arguments and don't have a key. For example
    *
    *  \code
    *  std::set<std::string> channels
    *     { "channel1" , "channel2" , "channel3" }
    *
    *  request req;
    *  req.push("SUBSCRIBE", std::cbegin(channels), std::cend(channels));
    *  \endcode
    *
    *  \param cmd The Redis command
    *  \param begin Iterator to the begin of the range.
    *  \param end Iterator to the end of the range.
    */
   template <class ForwardIterator>
   void push_range(std::string_view cmd, ForwardIterator begin, ForwardIterator end,
                   typename std::iterator_traits<ForwardIterator>::value_type * = nullptr)
   {
      using value_type = typename std::iterator_traits<ForwardIterator>::value_type;
      using resp3::type;

      if (begin == end)
         return;

      auto constexpr size = resp3::bulk_counter<value_type>::size;
      auto const distance = std::distance(begin, end);
      resp3::add_header(payload_, type::array, 1 + size * distance);
      resp3::add_bulk(payload_, cmd);

      for (; begin != end; ++begin)
	 resp3::add_bulk(payload_, *begin);

      check_cmd(cmd);
   }

   /** @brief Appends a new command to the end of the request.
    *  
    *  Equivalent to the overload taking a range (i.e. send_range2).
    *
    *  \param cmd Redis command.
    *  \param key Redis key.
    *  \param range Range to send e.g. and \c std::map.
    */
   template <class Key, class Range>
   void push_range(std::string_view cmd, Key const& key, Range const& range,
                   decltype(std::begin(range)) * = nullptr)
   {
      using std::begin;
      using std::end;
      push_range(cmd, key, begin(range), end(range));
   }

   /** @brief Appends a new command to the end of the request.
    *
    *  Equivalent to the overload taking a range (i.e. send_range2).
    *
    *  \param cmd Redis command.
    *  \param range Range to send e.g. and \c std::map.
    */
   template <class Range>
   void push_range(std::string_view cmd, Range const& range,
                   decltype(std::begin(range)) * = nullptr)
   {
      using std::begin;
      using std::end;
      push_range(cmd, begin(range), end(range));
   }

private:
   void check_cmd(std::string_view cmd)
   {
      if (!detail::has_response(cmd))
         ++commands_;

      if (cmd == "HELLO")
         has_hello_priority_ = cfg_.hello_with_priority;
   }

   config cfg_;
   std::string payload_;
   std::size_t commands_ = 0;
   bool has_hello_priority_ = false;
};

} // boost::redis::resp3

#endif // BOOST_REDIS_REQUEST_HPP
