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

namespace aedis {
namespace resp3 {

/** @brief A Redis request (also referred to as a pipeline).
 *  
 *  A request is composed of one or more redis commands and is
 *  referred to in the redis documentation as a pipeline, see
 *  https://redis.io/topics/pipelining.
 */
template <class Command>
class request {
private:
   std::string payload_;

public:
   /// The commands that have been queued in this request.
   std::queue<Command> commands;

public:
   /** Clears the request.
    *  
    *  Note: Already acquired memory won't be released. The is useful
    *  to reusing memory insteam of allocating again each time.
    */
   void clear()
   {
      payload_.clear();
      commands = {};
   }

   /** \brief Returns the payload the is written to the socket.
    */
   auto const& payload() const noexcept {return payload_;}

   /** @brief Appends a new command to end of the request.
    *
    *  Non-string types will be converted to string by using
    *  to_string which must be made available by the user.
    */
   template <class... Ts>
   void push(Command cmd, Ts const&... args)
   {
      // Note: Should we detect any std::pair in the type in the pack
      // to calculate the herader size correctly or let users handle
      // this?

      auto constexpr pack_size = sizeof...(Ts);
      detail::add_header(payload_, 1 + pack_size);

      detail::add_bulk(payload_, to_string(cmd));
      (detail::add_bulk(payload_, args), ...);

      if (!detail::has_push_response(cmd))
         commands.emplace(cmd);
   }

   /** @brief Appends a new command to end of the request.
       
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
   void push_range(Command cmd, Key const& key, ForwardIterator begin, ForwardIterator end)
   {
      // Note: For some commands like hset it would helpful to users
      // to assert the value type is a pair.

      using value_type = typename std::iterator_traits<ForwardIterator>::value_type;

      auto constexpr size = detail::value_type_size<value_type>::size;
      auto const distance = std::distance(begin, end);
      detail::add_header(payload_, 2 + size * distance);
      detail::add_bulk(payload_, to_string(cmd));
      detail::add_bulk(payload_, key);

      for (; begin != end; ++begin)
	 detail::add_bulk(payload_, *begin);

      if (!detail::has_push_response(cmd))
         commands.emplace(cmd);
   }

   /** @brief Appends a new command to end of the request.
     
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
   void push_range(Command cmd, ForwardIterator begin, ForwardIterator end)
   {
      // Note: For some commands like hset it would be a good idea to assert
      // the value type is a pair.

      using value_type = typename std::iterator_traits<ForwardIterator>::value_type;

      auto constexpr size = detail::value_type_size<value_type>::size;
      auto const distance = std::distance(begin, end);
      detail::add_header(payload_, 1 + size * distance);
      detail::add_bulk(payload_, to_string(cmd));

      for (; begin != end; ++begin)
	 detail::add_bulk(payload_, *begin);

      if (!detail::has_push_response(cmd))
         commands.emplace(cmd);
   }
};

// TODO: Incorporate this function in the documentation.

/** @brief Prepares the back of the queue to receive further commands. 
 *
 *  If true is returned the request in the front of the queue can be
 *  sent to the server. See async_write_some.
 */
template <class Queue>
bool prepare_next(Queue& reqs)
{
   if (std::empty(reqs)) {
      reqs.push({});
      return true;
   }

   if (std::size(reqs) == 1) {
      reqs.push({});
      return false;
   }

   return false;
}

} // resp3
} // aedis
