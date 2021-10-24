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

#include <aedis/command.hpp>
#include <aedis/resp3/detail/composer.hpp>

namespace aedis {
namespace resp3 {

/** A Redis request also referred to as a pipeline.
 *  
 *  A request is composed of one or more redis commands and is
 *  refered to in the redis documentation as a pipeline, see
 *  https://redis.io/topics/pipelining.
 *
 *  The protocol version suported is RESP3, see
 *  https://github.com/antirez/RESP3/blob/74adea588783e463c7e84793b325b088fe6edd1c/spec.md
 */
class request {
public:
   struct element {
      command cmd;
      std::string key;
   };
   std::string payload;
   std::queue<element> elements;

public:
   /// Return the size of the pipeline. i.e. how many commands it
   /// contains.
   auto size() const noexcept
      { return std::size(elements); }

   bool empty() const noexcept
      { return std::empty(payload); };

   /// Clears the request.
   void clear()
   {
      payload.clear();
      elements = {};
   }

   template <class... Ts>
   void push(command cmd, Ts const&... args)
   {
      // TODO: Calculate the size of any pair or tuple like type to
      // use in the header size.

      auto constexpr pack_size = sizeof...(Ts);
      detail::add_header(payload, 1 + pack_size);

      // TODO: as_string is not a good idea, better to_string.
      detail::add_bulk(payload, as_string(cmd));
      (detail::add_bulk(payload, args), ...);

      // TODO: Do not assume the front is convertible to a string.
      // TODO: Is it correct to use the front as the key.
      std::string_view key;
      if constexpr (pack_size != 0)
	 key = detail::front(args...);

      elements.emplace(cmd, std::string{key});
   }

   template <class ForwardIterator>
   void push_range(command cmd, std::string_view key, ForwardIterator begin, ForwardIterator end)
   {
      // TODO: For hset find a way to assert the value type is a pair.
      using value_type = typename std::iterator_traits<ForwardIterator>::value_type;

      auto constexpr size = detail::value_type_size<value_type>::size;
      auto const distance = std::distance(begin, end);
      detail::add_header(payload, 2 + size * distance);
      detail::add_bulk(payload, as_string(cmd));
      detail::add_bulk(payload, key);

      for (; begin != end; ++begin)
	 detail::add_bulk(payload, *begin);

      elements.emplace(cmd, std::string{key});
   }

   /// Adds subscribe to the request, see https://redis.io/commands/subscribe
   void subscribe(std::initializer_list<std::string_view> keys)
   {
      // The response to this command is a push type.
      detail::assemble(payload, "SUBSCRIBE", keys);
   }

   void psubscribe(std::initializer_list<std::string_view> keys)
   {
      detail::assemble(payload, "PSUBSCRIBE", keys);
   }
   
   void unsubscribe(std::string_view key)
   {
      // The response to this command is a push type.
      detail::assemble(payload, "UNSUBSCRIBE", key);
   }
};

/** Prepares the back of a queue to receive further commands and
 *  returns true if a write is possible.
 */
bool prepare_next(std::queue<request>& reqs);

/** Writes the request element as a string to the stream.
 */
std::ostream& operator<<(std::ostream& os, request::element const& r);

} // resp3
} // aedis
