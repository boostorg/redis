/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_RESP3_REQUEST_HPP
#define AEDIS_RESP3_REQUEST_HPP

#include <aedis/resp3/compose.hpp>
#include <boost/hana.hpp>
#include <boost/utility/string_view.hpp>

// NOTE: Consider detecting tuples in the type in the parameter pack
// to calculate the header size correctly.
//
// NOTE: For some commands like hset it would be a good idea to assert
// the value type is a pair.

namespace aedis {
namespace resp3 {
namespace detail {

bool has_push_response(boost::string_view cmd);

} // detail

/** @brief Creates Redis requests.
 *  \ingroup any
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
 *  co_await async_write(socket, buffer(r));
 *  @endcode
 *
 *  \remarks  Non-string types will be converted to string by using \c
 *  to_bulk, which must be made available over ADL.
 */
class request {
public:
   //// Returns the number of commands contained in this request.
   std::size_t size() const noexcept { return commands_;};

   // Returns the request payload.
   auto const& payload() const noexcept { return payload_;}

   /// Clears the request preserving allocated memory.
   void clear()
   {
      payload_.clear();
      commands_ = 0;
   }

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
   void push(boost::string_view cmd, Ts const&... args)
   {
      using boost::hana::for_each;
      using boost::hana::make_tuple;
      using resp3::type;

      auto constexpr pack_size = sizeof...(Ts);
      resp3::add_header(payload_, type::array, 1 + pack_size);
      resp3::add_bulk(payload_, cmd);
      resp3::add_bulk(payload_, make_tuple(args...));

      if (!detail::has_push_response(cmd))
         ++commands_;
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
    *  req.push_range2("HSET", "key", std::cbegin(map), std::cend(map));
    *  @endcode
    *  
    *  \param cmd The command e.g. Redis or Sentinel command.
    *  \param key The command key.
    *  \param begin Iterator to the begin of the range.
    *  \param end Iterator to the end of the range.
    */
   template <class Key, class ForwardIterator>
   void push_range2(boost::string_view cmd, Key const& key, ForwardIterator begin, ForwardIterator end)
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

      if (!detail::has_push_response(cmd))
         ++commands_;
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
    *  req.push("SUBSCRIBE", std::cbegin(channels), std::cedn(channels));
    *  \endcode
    *
    *  \param cmd The Redis command
    *  \param begin Iterator to the begin of the range.
    *  \param end Iterator to the end of the range.
    */
   template <class ForwardIterator>
   void push_range2(boost::string_view cmd, ForwardIterator begin, ForwardIterator end)
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

      if (!detail::has_push_response(cmd))
         ++commands_;
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
   void push_range(boost::string_view cmd, Key const& key, Range const& range)
   {
      using std::begin;
      using std::end;
      push_range2(cmd, key, begin(range), end(range));
   }

   /** @brief Appends a new command to the end of the request.
    *
    *  Equivalent to the overload taking a range (i.e. send_range2).
    *
    *  \param cmd Redis command.
    *  \param range Range to send e.g. and \c std::map.
    */
   template <class Range>
   void push_range(boost::string_view cmd, Range const& range)
   {
      using std::begin;
      using std::end;
      push_range2(cmd, begin(range), end(range));
   }

   mutable bool close_on_run_completion = false;

private:
   std::string payload_;
   std::size_t commands_ = 0;
};

} // resp3
} // aedis

#endif // AEDIS_RESP3_SERIALIZER_HPP
