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

#include <aedis/command.hpp>
#include <aedis/resp3/detail/composer.hpp>

namespace aedis {
namespace resp3 {

/** A Redis request (also referred to as a pipeline).
 *  
 *  A request is composed of one or more redis commands and is
 *  referred to in the redis documentation as a pipeline, see
 *  https://redis.io/topics/pipelining.
 */
class request {
public:
   std::string payload;
   std::queue<command> commands;

public:
   /** Returns the size of the command pipeline. i.e. how many
    * commands it contains.
   */
   auto size() const noexcept
      { return std::size(commands); }

   /** Return true if the request contains no commands.
   */
   bool empty() const noexcept
      { return std::empty(payload); };

   /// Clears the request.
   void clear()
   {
      payload.clear();
      commands = {};
   }

   /** Appends a new command to the request.
    *
    *  Non-string types will be converted to string by using
    *  to_string which must be made available by the user.
    */
   template <class... Ts>
   void push(command cmd, Ts const&... args)
   {
      // Note: Should we detect any std::pair in the type in the pack
      // to calculate the herader size correctly or let users handle
      // this?

      auto constexpr pack_size = sizeof...(Ts);
      detail::add_header(payload, 1 + pack_size);

      detail::add_bulk(payload, to_string(cmd));
      (detail::add_bulk(payload, args), ...);

      if (!detail::has_push_response(cmd))
         commands.emplace(cmd);
   }

   /** Appends a new command to the request.
    */
   template <class Key, class ForwardIterator>
   void push_range(command cmd, Key const& key, ForwardIterator begin, ForwardIterator end)
   {
      // Note: For some commands like hset it would helpful to users
      // to assert the value type is a pair.

      using value_type = typename std::iterator_traits<ForwardIterator>::value_type;

      auto constexpr size = detail::value_type_size<value_type>::size;
      auto const distance = std::distance(begin, end);
      detail::add_header(payload, 2 + size * distance);
      detail::add_bulk(payload, to_string(cmd));
      detail::add_bulk(payload, key);

      for (; begin != end; ++begin)
	 detail::add_bulk(payload, *begin);

      if (!detail::has_push_response(cmd))
         commands.emplace(cmd);
   }

   /** Appends a new command to the request.
    */
   template <class ForwardIterator>
   void push_range(command cmd, ForwardIterator begin, ForwardIterator end)
   {
      // Note: For some commands like hset it would be a good idea to assert
      // the value type is a pair.

      using value_type = typename std::iterator_traits<ForwardIterator>::value_type;

      auto constexpr size = detail::value_type_size<value_type>::size;
      auto const distance = std::distance(begin, end);
      detail::add_header(payload, 1 + size * distance);
      detail::add_bulk(payload, to_string(cmd));

      for (; begin != end; ++begin)
	 detail::add_bulk(payload, *begin);

      if (!detail::has_push_response(cmd))
         commands.emplace(cmd);
   }
};

/** Prepares the back of a queue to receive further commands.
 *
 *  If true is returned the request in the front of the queue can be
 *  sent to the server. See async_write_some.
 */
template <class Queue>
bool prepare_next(Queue& reqs)
{
   auto const cond = std::empty(reqs) || std::size(reqs) == 1;
   if (cond)
      reqs.push({});

   return cond;
}

} // resp3
} // aedis
