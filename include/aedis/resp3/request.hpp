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

   void push(command c)
   {
      detail::add_header(payload, 1);
      detail::add_bulk(payload, as_string(c));
      elements.emplace(c, std::string{});
   }

   void push(command c, std::string_view param)
   {
      detail::add_header(payload, 2);
      detail::add_bulk(payload, as_string(c));
      detail::add_bulk(payload, param);
      elements.emplace(c, std::string{param});
   }

   void push(command c, std::string_view p1, std::string_view p2)
   {
      detail::add_header(payload, 3);
      detail::add_bulk(payload, as_string(c));
      detail::add_bulk(payload, p1);
      detail::add_bulk(payload, p2);
      elements.emplace(c, std::string{p1});
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

   /// Adds ping to the request, see https://redis.io/commands/sentinel
   void sentinel(std::string_view arg, std::string_view name)
   {
      auto const par = {name};
      auto const args = {arg};
      detail::assemble(payload, "SENTINEL", std::cbegin(args), std::cend(args), std::cbegin(par), std::cend(par));
      elements.emplace(command::sentinel, std::string{});
   }
   
   /// Adds ping to the request, see https://redis.io/commands/bitcount
   void bitcount(std::string_view key, int start = 0, int end = -1)
   {
      auto const start_str = std::to_string(start);
      auto const end_str = std::to_string(end);
      auto const param = {start_str, end_str};
      auto const keys = {key};

      detail::assemble(payload, "BITCOUNT", keys, param);
      elements.emplace(command::bitcount, std::string{key});
   }
   
   /// Adds ping to the request, see https://redis.io/commands/rpush
   template <class Iter>
   void rpush(std::string_view key, Iter begin, Iter end)
   {
      auto const keys = {key};
      detail::assemble(payload, "RPUSH", std::cbegin(keys), std::cend(keys), begin, end);
      elements.emplace(command::rpush, std::string{key});
   }
   
   /// Adds ping to the request, see https://redis.io/commands/rpush
   template <class T>
   void rpush(std::string_view key, std::initializer_list<T> v)
   {
      return rpush(key, std::cbegin(v), std::cend(v));
   }

   /// Adds ping to the request, see https://redis.io/commands/rpush
   template <class Range>
   void rpush(std::string_view key, Range const& v)
   {
      using std::cbegin;
      using std::cend;
      rpush(key, cbegin(v), cend(v));
   }
   
   /// Adds ping to the request, see https://redis.io/commands/lpush
   template <class Iter>
   void lpush(std::string_view key, Iter begin, Iter end)
   {
      detail::assemble(payload, "LPUSH", {key}, begin, end);
      elements.emplace(command::lpush, std::string{key});
   }
   
   /// Adds ping to the request, see https://redis.io/commands/publish
   void publish(std::string_view key, std::string_view msg)
   {
      auto const par = {msg};
      auto const keys = {key};
      detail::assemble(payload, "PUBLISH", std::cbegin(keys), std::cend(keys), std::cbegin(par), std::cend(par));
      elements.emplace(command::publish, std::string{key});
   }
   
   /// Adds ping to the request, see https://redis.io/commands/set
   void set(std::string_view key,
            std::initializer_list<std::string_view> args)
   {
      auto const keys = {key};
      detail::assemble(payload, "SET", std::cbegin(keys), std::cend(keys), std::cbegin(args), std::cend(args));
      elements.emplace(command::set, std::string{key});
   }

   // TODO: Find a way to assert the value type is a pair.
   /// Adds ping to the request, see https://redis.io/commands/hset
   template <class Range>
   void hset(std::string_view key, Range const& r)
   {
      //Note: Requires an std::pair as value type, otherwise gets
      //error: ERR Protocol error: expected '$', got '*'
      using std::cbegin;
      using std::cend;
      auto const keys = {key};
      detail::assemble(payload, "HSET", std::cbegin(keys), std::cend(keys), std::cbegin(r), std::cend(r));
      elements.emplace(command::hset, std::string{key});
   }
   
   /// Adds ping to the request, see https://redis.io/commands/hincrby
   void hincrby(std::string_view key, std::string_view field, int by)
   {
      auto const by_str = std::to_string(by);
      std::initializer_list<std::string_view> par {field, by_str};
      auto const keys = {key};
      detail::assemble(payload, "HINCRBY", std::cbegin(keys), std::cend(keys),
          std::cbegin(par), std::cend(par));
      elements.emplace(command::hincrby, std::string{key});
   }
   
   /// Adds ping to the request, see https://redis.io/commands/hget
   void hget(std::string_view key, std::string_view field)
   {
      auto const par = {field};
      auto const keys = {key};
      detail::assemble(payload, "HGET", std::cbegin(keys), std::cend(keys), std::cbegin(par), std::cend(par));
      elements.emplace(command::hget, std::string{key});
   }
   
   /// Adds ping to the request, see https://redis.io/commands/hmget
   void hmget(
      std::string_view key,
      std::initializer_list<std::string_view> fields)
   {
      auto const keys = {key};
      detail::assemble( payload
   	            , "HMGET"
   		    , std::cbegin(keys), std::cend(keys)
   		    , std::cbegin(fields)
   		    , std::cend(fields));

      elements.emplace(command::hmget, std::string{key});
   }
   
   /// Adds ping to the request, see https://redis.io/commands/hdel
   void
   hdel(std::string_view key,
        std::initializer_list<std::string_view> fields)
   {
      auto const keys = {key};
      detail::assemble(
	 payload,
	 "HDEL",
         std::cbegin(keys), std::cend(keys),
	 std::cbegin(fields),
	 std::cend(fields));

      elements.emplace(command::hdel, std::string{key});
   }

   /// Adds ping to the request, see https://redis.io/commands/expire
   void expire(std::string_view key, int secs)
   {
      auto const str = std::to_string(secs);
      std::initializer_list<std::string_view> par {str};

      auto const keys = {key};
      detail::assemble(payload, "EXPIRE", std::cbegin(keys), std::cend(keys), std::cbegin(par), std::cend(par));
      elements.emplace(command::expire, std::string{key});
   }
   
   /// Adds ping to the request, see https://redis.io/commands/zadd
   void zadd(std::string_view key, int score, std::string_view value)
   {
      auto const score_str = std::to_string(score);
      std::initializer_list<std::string_view> par = {score_str, value};
      auto const keys = {key};
      detail::assemble(payload, "ZADD", std::cbegin(keys), std::cend(keys), std::cbegin(par), std::cend(par));
      elements.emplace(command::zadd, std::string{key});
   }
   
   /// Adds zadd to the request, see https://redis.io/commands/zadd
   template <class Range>
   void zadd(std::initializer_list<std::string_view> key, Range const& r)
   {
      detail::assemble(payload, "ZADD", key, std::cbegin(r), std::cend(r), 2);
      elements.emplace(command::zadd, std::string{key});
   }
   
   /// Adds ping to the request, see https://redis.io/commands/zrange
   void zrange(std::string_view key, int min = 0, int max = -1)
   {
      auto const min_str = std::to_string(min);
      auto const max_str = std::to_string(max);
      std::initializer_list<std::string_view> par {min_str, max_str};

      auto const keys = {key};
      detail::assemble(payload, "ZRANGE", std::cbegin(keys), std::cend(keys), std::cbegin(par), std::cend(par));
      elements.emplace(command::zrange, std::string{key});
   }
   
   /// Adds ping to the request, see https://redis.io/commands/zrangebyscore
   void zrangebyscore(std::string_view key, int min, int max)
   {
      auto max_str = std::string {"inf"};
      if (max != -1)
         max_str = std::to_string(max);
   
      auto const min_str = std::to_string(min);
      auto par = {min_str , max_str};
      auto const keys = {key};
      detail::assemble(payload, "ZRANGEBYSCORE", std::cbegin(keys), std::cend(keys), std::cbegin(par), std::cend(par));
      elements.emplace(command::zrangebyscore, std::string{key});
   }
   
   /// Adds ping to the request, see https://redis.io/commands/zremrangebyscore
   void
   zremrangebyscore(
      std::string_view key,
      std::string_view min,
      std::string_view max)
   {
      auto const par = {min, max};
      auto const keys = {key};
      detail::assemble(payload, "ZREMRANGEBYSCORE", std::cbegin(keys), std::cend(keys), std::cbegin(par), std::cend(par));
      elements.emplace(command::zremrangebyscore, std::string{key});
   }
   
   /// Adds ping to the request, see https://redis.io/commands/lrange
   void lrange(std::string_view key, int min = 0, int max = -1)
   {
      auto const min_str = std::to_string(min);
      auto const max_str = std::to_string(max);
      std::initializer_list<std::string_view> par {min_str, max_str};
      auto const keys = {key};
      detail::assemble(payload, "LRANGE", std::cbegin(keys), std::cend(keys), std::cbegin(par), std::cend(par));
      elements.emplace(command::lrange, std::string{key});
   }
   
   /// Adds ping to the request, see https://redis.io/commands/ltrim
   void ltrim(std::string_view key, int min = 0, int max = -1)
   {
      auto const min_str = std::to_string(min);
      auto const max_str = std::to_string(max);
      std::initializer_list<std::string_view> par {min_str, max_str};
      auto const keys = {key};
      detail::assemble(payload, "LTRIM", std::cbegin(keys), std::cend(keys), std::cbegin(par), std::cend(par));
      elements.emplace(command::ltrim, std::string{key});
   }
   
   /// Adds ping to the request, see https://redis.io/commands/sadd
   template <class Iter>
   void sadd(std::string_view key, Iter begin, Iter end)
   {
      auto const keys = {key};
      detail::assemble(payload, "SADD", std::cbegin(keys), std::cend(keys), begin, end);
      elements.emplace(command::sadd, std::string{key});
   }

   /// Adds ping to the request, see https://redis.io/commands/sadd
   template <class Range>
   void sadd(std::string_view key, Range const& r)
   {
      using std::cbegin;
      using std::cend;
      sadd(key, cbegin(r), cend(r));
   }

   /// Adds ping to the request, see https://redis.io/commands/scard
   void scard(std::string_view key, std::initializer_list<std::string_view> l)
   {
      auto const keys = {key};
      detail::assemble(payload, "SDIFF", std::cbegin(keys), std::cend(keys), std::cbegin(l), std::cend(l));
      elements.emplace(command::sdiff, std::string{key});
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
