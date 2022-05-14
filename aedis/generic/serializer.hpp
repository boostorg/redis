/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_GENERIC_SERIALIZER_HPP
#define AEDIS_GENERIC_SERIALIZER_HPP

#include <boost/hana.hpp>
#include <aedis/resp3/compose.hpp>

// NOTE: Consider detecting tuples in the type in the parameter pack
// to calculate the header size correctly.
//
// NOTE: For some commands like hset it would be a good idea to assert
// the value type is a pair.

namespace aedis {
namespace generic {

/** @brief Creates Redis requests from user data.
 *  \ingroup any
 *  
 *  A request is composed of one or more redis commands and is
 *  referred to in the redis documentation as a pipeline, see
 *  https://redis.io/topics/pipelining.
 *
 *  Example
 *
 *  @code
 *  request r;
 *  r.push(command::hello, 3);
 *  r.push(command::flushall);
 *  r.push(command::ping);
 *  r.push(command::incr, "key");
 *  r.push(command::quit);
 *  co_await async_write(socket, buffer(r));
 *  @endcode
 *
 *  \tparam Storage The storage type e.g \c std::string.
 *
 *  \remarks  Non-string types will be converted to string by using \c
 *  to_bulk, which must be made available over ADL.
 */
template <class Command>
class serializer {
public:
   using command_info_type = std::pair<Command, std::size_t>;

private:

   std::string payload_;
   std::vector<command_info_type> commands_;

public:
   auto const& commands() const { return commands_;};
   auto size() const { return payload_.size();}
   auto empty() const { return payload_.empty(); }
   auto const* data() const { return payload_.data(); }
   auto const& payload() const { return payload_;}

   void pop()
   {
      BOOST_ASSERT(!commands_.empty());
      commands_.erase(std::begin(commands_));

      // TODO: Erase the payload, perhaps by adding an offset of
      // already acknolodged commands.
      // TODO: Add function to enable laze pop.
   }

   /** @brief Appends a new command to the end of the request.
    *
    *  For example
    *
    *  \code
    *  std::string request;
    *  auto sr = make_serializer<command>(request);
    *  sr.push(command::set, "key", "some string", "EX", "2");
    *  \endcode
    *
    *  will add the \c set command with value "some string" and an
    *  expiration of 2 seconds.
    *
    *  \param cmd The command e.g redis or sentinel command.
    *  \param args Command arguments.
    */
   template <class... Ts>
   void push(Command cmd, Ts const&... args)
   {
      using boost::hana::for_each;
      using boost::hana::make_tuple;
      using resp3::type;

      auto const before = payload_.size();
      auto constexpr pack_size = sizeof...(Ts);
      resp3::add_header(payload_, type::array, 1 + pack_size);
      resp3::add_bulk(payload_, to_string(cmd));
      resp3::add_bulk(payload_, make_tuple(args...));

      auto const after = payload_.size();
      if (!has_push_response(cmd))
         commands_.push_back(std::make_pair(cmd, after - before));
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
    *  req.push_range2(command::hset, "key", std::cbegin(map), std::cend(map));
    *  @endcode
    *  
    *  \param cmd The command e.g. Redis or Sentinel command.
    *  \param key The command key.
    *  \param begin Iterator to the begin of the range.
    *  \param end Iterator to the end of the range.
    */
   template <class Key, class ForwardIterator>
   void push_range2(Command cmd, Key const& key, ForwardIterator begin, ForwardIterator end)
   {
      using value_type = typename std::iterator_traits<ForwardIterator>::value_type;
      using resp3::type;

      if (begin == end)
         return;

      auto const before = payload_.size();

      auto constexpr size = resp3::bulk_counter<value_type>::size;
      auto const distance = std::distance(begin, end);
      resp3::add_header(payload_, type::array, 2 + size * distance);
      resp3::add_bulk(payload_, to_string(cmd));
      resp3::add_bulk(payload_, key);

      for (; begin != end; ++begin)
	 resp3::add_bulk(payload_, *begin);

      auto const after = payload_.size();
      if (!has_push_response(cmd))
         commands_.push_back(std::make_pair(cmd, after - before));
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
    *  req.push(command::subscribe, std::cbegin(channels), std::cedn(channels));
    *  \endcode
    *
    *  \param cmd The Redis command
    *  \param begin Iterator to the begin of the range.
    *  \param end Iterator to the end of the range.
    */
   template <class ForwardIterator>
   void push_range2(Command cmd, ForwardIterator begin, ForwardIterator end)
   {
      using value_type = typename std::iterator_traits<ForwardIterator>::value_type;
      using resp3::type;

      if (begin == end)
         return;

      auto const before = payload_.size();
      auto constexpr size = resp3::bulk_counter<value_type>::size;
      auto const distance = std::distance(begin, end);
      resp3::add_header(payload_, type::array, 1 + size * distance);
      resp3::add_bulk(payload_, to_string(cmd));

      for (; begin != end; ++begin)
	 resp3::add_bulk(payload_, *begin);

      auto const after = payload_.size();
      if (!has_push_response(cmd))
         commands_.push_back(std::make_pair(cmd, before - after));
   }

   /** @brief Appends a new command to the end of the request.
    *  
    *  Equivalent to the overload taking a range (i.e. send_range2).
    */
   template <class Key, class Range>
   void push_range(Command cmd, Key const& key, Range const& range)
   {
      using std::begin;
      using std::end;
      push_range2(cmd, key, begin(range), end(range));
   }

   /** @brief Appends a new command to the end of the request.
    *
    *  Equivalent to the overload taking a range (i.e. send_range2).
    */
   template <class Range>
   void push_range(Command cmd, Range const& range)
   {
      using std::begin;
      using std::end;
      push_range2(cmd, begin(range), end(range));
   }
};

} // generic
} // aedis

#endif // AEDIS_GENERIC_SERIALIZER_HPP
