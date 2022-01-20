/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <queue>
#include <vector>
#include <string>
#include <string_view>
#include <utility>
#include <ostream>
#include <iterator>

#include <aedis/resp3/detail/composer.hpp>
#include <aedis/command.hpp>

namespace aedis {
namespace resp3 {

/** @brief Creates a RESP3 request from user data.
 *  \ingroup classes
 *  
 *  A request is composed of one or more redis commands and is
 *  referred to in the redis documentation as a pipeline, see
 *  https://redis.io/topics/pipelining, for example
 *
 *  @code
 *  serializer<command> sr;
 *  sr.push(command::hello, 3);
 *  sr.push(command::flushall);
 *  sr.push(command::ping);
 *  sr.push(command::incr, "key");
 *  sr.push(command::quit);
 *  co_await async_write(socket, buffer(sr.request()));
 *  @endcode
 */
template <class Container, class Command>
class serializer {
private:
   Container* request_;

public:
   /// Constructor
   serializer(Container& container) : request_(&container) {}

   /** @brief Appends a new command to the end of the request.
    *
    *  Non-string types will be converted to string by using
    *  \c to_string, which must be made available by the user by ADL.
    */
   template <class... Ts>
   void push(Command qelem, Ts const&... args)
   {
      // TODO: Should we detect any std::pair in the type in the pack
      // to calculate the header size correctly?

      auto constexpr pack_size = sizeof...(Ts);
      detail::add_header(*request_, 1 + pack_size);

      auto const cmd = detail::request_get_command<Command>::apply(qelem);
      detail::add_bulk(*request_, to_string(cmd));
      (detail::add_bulk(*request_, args), ...);
   }

   /** @brief Appends a new command to the end of the request.
       
       This overload is useful for commands that have a key. For example
     
       \code{.cpp}
       std::map<std::string, std::string> map
	  { {"key1", "value1"}
	  , {"key2", "value2"}
	  , {"key3", "value3"}
	  };

       request req;
       req.push_range(command::hset, "key", std::cbegin(map), std::cend(map));
       \endcode
    */
   template <class Key, class ForwardIterator>
   void push_range(Command qelem, Key const& key, ForwardIterator begin, ForwardIterator end)
   {
      // Note: For some commands like hset it would helpful to users
      // to assert the value type is a pair.

      using value_type = typename std::iterator_traits<ForwardIterator>::value_type;

      auto constexpr size = detail::value_type_size<value_type>::size;
      auto const distance = std::distance(begin, end);
      detail::add_header(*request_, 2 + size * distance);
      auto const cmd = detail::request_get_command<Command>::apply(qelem);
      detail::add_bulk(*request_, to_string(cmd));
      detail::add_bulk(*request_, key);

      for (; begin != end; ++begin)
	 detail::add_bulk(*request_, *begin);
   }

   /** @brief Appends a new command to the end of the request.
     
       This overload is useful for commands that don't have a key. For
       example
     
       \code{.cpp}
       std::set<std::string> channels
	  { "channel1" , "channel2" , "channel3" }

       request req;
       req.push(command::subscribe, std::cbegin(channels), std::cedn(channels));
       \endcode
    */
   template <class ForwardIterator>
   void push_range(Command qelem, ForwardIterator begin, ForwardIterator end)
   {
      // Note: For some commands like hset it would be a good idea to assert
      // the value type is a pair.

      using value_type = typename std::iterator_traits<ForwardIterator>::value_type;

      auto constexpr size = detail::value_type_size<value_type>::size;
      auto const distance = std::distance(begin, end);
      detail::add_header(*request_, 1 + size * distance);
      auto const cmd = detail::request_get_command<Command>::apply(qelem);
      detail::add_bulk(*request_, to_string(cmd));

      for (; begin != end; ++begin)
	 detail::add_bulk(*request_, *begin);
   }
};

/** \brief Creates a serializer from a container.
 *  \ingroup functions
 *  TODO: Add the string template parameters.
 */
template <class Command>
auto make_serializer(std::string& container)
{
   return serializer<std::string, Command>(container);
}

} // resp3
} // aedis
