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
#include <numeric>
#include <algorithm>
#include <functional>
#include <type_traits>
#include <string_view>

#include <aedis/detail/commands.hpp>

#include "config.hpp"

namespace aedis { namespace resp {

inline
void add_bulk(std::string& to, std::string_view param)
{
   to += "$";
   to += std::to_string(std::size(param));
   to += "\r\n";
   to += param;
   to += "\r\n";
}

inline
void add_header(std::string& to, int size)
{
   to += "*";
   to += std::to_string(size);
   to += "\r\n";
}

struct accumulator {
   auto
   operator()(
      std::string a,
      std::string_view b) const
   {
      add_bulk(a, b);
      return a;
   }

   template <class T>
   auto
   operator()(
      std::string a,
      T b,
      typename std::enable_if<(std::is_integral<T>::value || std::is_floating_point<T>::value), bool>::type = false) const
   {
      auto const v = std::to_string(b);
      add_bulk(a, v);
      return a;
   }

   auto
   operator()(
      std::string a,
      std::pair<std::string, std::string_view> b) const
   {
      add_bulk(a, b.first);
      add_bulk(a, b.second);
      return a;
   }

   template <class T>
   auto
   operator()(
      std::string a,
      std::pair<T, std::string_view> b,
      typename std::enable_if<(std::is_integral<T>::value || std::is_floating_point<T>::value), bool>::type = false) const
   {
      auto const v = std::to_string(b.first);
      add_bulk(a, v);
      add_bulk(a, b.second);
      return a;
   }
};

inline
void assemble(std::string& ret, std::string_view cmd)
{
   add_header(ret, 1);
   add_bulk(ret, cmd);
}

template <class Iter>
void assemble( std::string& ret
             , std::string_view cmd
             , std::initializer_list<std::string_view> key
             , Iter begin
             , Iter end
             , int size = 1)
{
   auto const d1 =
      std::distance( std::cbegin(key)
                   , std::cend(key));

   auto const d2 = std::distance(begin, end);

   std::string a;
   add_header(a, 1 + d1 + size * d2);
   add_bulk(a, cmd);

   auto b =
      std::accumulate( std::cbegin(key)
                     , std::cend(key)
                     , std::move(a)
                     , accumulator{});

   ret +=
      std::accumulate( begin
                     , end
                     , std::move(b)
                     , accumulator{});
}

inline
void assemble(std::string& ret, std::string_view cmd, std::string_view key)
{
   std::initializer_list<std::string_view> dummy;
   assemble(ret, cmd, {key}, std::cbegin(dummy), std::cend(dummy));
}

} // resp

/** A class to compose redis requests
 *  
 *  A request is composed of one or more redis commands and is refered
 *  to in the redis documentation as a pipeline, see
 *  https://redis.io/topics/pipelining.
 *
 *  The protocol version suported is RESP3, see
 *  https://github.com/antirez/RESP3/blob/74adea588783e463c7e84793b325b088fe6edd1c/spec.md
 */
class request {
public:
   std::string payload;
   std::queue<commands> cmds;

public:
   bool empty() const noexcept { return std::empty(payload); };

   /// Clears the request.
   void clear()
   {
      payload.clear();
      cmds = {};
   }

   /// Adds ping to the request, see https://redis.io/commands/bgrewriteaof
   void ping()
   {
      resp::assemble(payload, "PING");
      cmds.push(commands::ping);
   }

   /// Adds quit to the request, see https://redis.io/commands/quit
   void quit()
   {
      resp::assemble(payload, "QUIT");
      cmds.push(commands::quit);
   }

   /// Adds multi to the request, see https://redis.io/commands/multi
   void multi()
   {
      resp::assemble(payload, "MULTI");
      cmds.push(commands::multi);
   }

   /// Adds exec to the request, see https://redis.io/commands/exec
   void exec()
   {
      resp::assemble(payload, "EXEC");
      cmds.push(commands::exec);
   }

   /// Adds incr to the request, see https://redis.io/commands/incr
   void incr(std::string_view key)
   {
      resp::assemble(payload, "INCR", key);
      cmds.push(commands::incr);
   }

   /// Adds auth to the request, see https://redis.io/commands/bgrewriteaof
   void auth(std::string_view pwd)
   {
      resp::assemble(payload, "AUTH", pwd);
      cmds.push(commands::auth);
   }

   /// Adds bgrewriteaof to the request, see https://redis.io/commands/bgrewriteaof
   void bgrewriteaof()
   {
      resp::assemble(payload, "BGREWRITEAOF");
      cmds.push(commands::bgrewriteaof);
   }

   /// Adds role to the request, see https://redis.io/commands/role
   void role()
   {
      resp::assemble(payload, "ROLE");
      cmds.push(commands::role);
   }

   /// Adds bgsave to the request, see //https://redis.io/commands/bgsave
   void bgsave()
   {
      resp::assemble(payload, "BGSAVE");
      cmds.push(commands::bgsave);
   }

   /// Adds ping to the request, see https://redis.io/commands/flushall
   void flushall()
   {
      resp::assemble(payload, "FLUSHALL");
      cmds.push(commands::flushall);
   }

   /// Adds ping to the request, see https://redis.io/commands/lpop
   void lpop(std::string_view key, int count = 1)
   {
      //if (count == 1) {
	 resp::assemble(payload, "LPOP", key);
      //} else {
      //auto par = {std::to_string(count)};
      //resp::assemble(
      //   payload,
      //   "LPOP",
      //   {key},
      //   std::cbegin(par),
      //   std::cend(par));
      //}

      cmds.push(commands::lpop);
   }

   /// Adds ping to the request, see https://redis.io/commands/subscribe
   void subscribe(std::string_view key)
   {
      // The response to this command is a push.
      resp::assemble(payload, "SUBSCRIBE", key);
   }

   /// Adds ping to the request, see https://redis.io/commands/unsubscribe
   void unsubscribe(std::string_view key)
   {
      // The response to this command is a push.
      resp::assemble(payload, "UNSUBSCRIBE", key);
   }

   /// Adds ping to the request, see https://redis.io/commands/get
   void get(std::string_view key)
   {
      resp::assemble(payload, "GET", key);
      cmds.push(commands::get);
   }

   /// Adds ping to the request, see https://redis.io/commands/keys
   void keys(std::string_view pattern)
   {
      resp::assemble(payload, "KEYS", pattern);
      cmds.push(commands::keys);
   }

   /// Adds ping to the request, see https://redis.io/commands/hello
   void hello(std::string_view version = "3")
   {
      resp::assemble(payload, "HELLO", version);
      cmds.push(commands::hello);
   }
   
   /// Adds ping to the request, see https://redis.io/commands/sentinel
   void sentinel(std::string_view arg, std::string_view name)
   {
      auto par = {name};
      resp::assemble(payload, "SENTINEL", {arg}, std::cbegin(par), std::cend(par));
      cmds.push(commands::sentinel);
   }
   
   /// Adds ping to the request, see https://redis.io/commands/append
   void append(std::string_view key, std::string_view msg)
   {
      auto par = {msg};
      resp::assemble(payload, "APPEND", {key}, std::cbegin(par), std::cend(par));
      cmds.push(commands::append);
   }
   
   /// Adds ping to the request, see https://redis.io/commands/bitcount
   void bitcount(std::string_view key, int start = 0, int end = -1)
   {
      auto const start_str = std::to_string(start);
      auto const end_str = std::to_string(end);
      std::initializer_list<std::string_view> par {start_str, end_str};

      resp::assemble( payload
   	            , "BITCOUNT"
   		    , {key}
   		    , std::cbegin(par)
   		    , std::cend(par));
      cmds.push(commands::bitcount);
   }
   
   /// Adds ping to the request, see https://redis.io/commands/rpush
   template <class Iter>
   void rpush(std::string_view key, Iter begin, Iter end)
   {
      resp::assemble(payload, "RPUSH", {key}, begin, end);
      cmds.push(commands::rpush);
   }
   
   /// Adds ping to the request, see https://redis.io/commands/rpush
   template <class T>
   void rpush( std::string_view key, std::initializer_list<T> v)
   {
      return rpush(key, std::cbegin(v), std::cend(v));
   }

   /// Adds ping to the request, see https://redis.io/commands/rpush
   template <class Range>
   void rpush( std::string_view key, Range const& v)
   {
      using std::cbegin;
      using std::cend;
      rpush(key, cbegin(v), cend(v));
   }
   
   /// Adds ping to the request, see https://redis.io/commands/lpush
   template <class Iter>
   void lpush(std::string_view key, Iter begin, Iter end)
   {
      resp::assemble(payload, "LPUSH", {key}, begin, end);
      cmds.push(commands::lpush);
   }
   
   /// Adds ping to the request, see https://redis.io/commands/psubscribe
   void psubscribe( std::initializer_list<std::string_view> l)
   {
      std::initializer_list<std::string_view> dummy = {};
      resp::assemble(payload, "PSUBSCRIBE", l, std::cbegin(dummy), std::cend(dummy));
   }
   
   /// Adds ping to the request, see https://redis.io/commands/publish
   void publish(std::string_view key, std::string_view msg)
   {
      auto par = {msg};
      resp::assemble(payload, "PUBLISH", {key}, std::cbegin(par), std::cend(par));
      cmds.push(commands::publish);
   }
   
   /// Adds ping to the request, see https://redis.io/commands/set
   void set(std::string_view key,
            std::initializer_list<std::string_view> args)
   {
      resp::assemble(payload, "SET", {key}, std::cbegin(args), std::cend(args));
      cmds.push(commands::set);
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
      resp::assemble(payload, "HSET", {key}, std::cbegin(r), std::cend(r), 2);
      cmds.push(commands::hset);
   }
   
   /// Adds ping to the request, see https://redis.io/commands/hincrby
   void hincrby(std::string_view key, std::string_view field, int by)
   {
      auto by_str = std::to_string(by);
      std::initializer_list<std::string_view> par {field, by_str};
      resp::assemble(payload, "HINCRBY", {key}, std::cbegin(par), std::cend(par));
      cmds.push(commands::hincrby);
   }
   
   /// Adds ping to the request, see https://redis.io/commands/hkeys
   void hkeys(std::string_view key)
   {
      auto par = {""};
      resp::assemble(payload, "HKEYS", {key}, std::cbegin(par), std::cend(par));
      cmds.push(commands::hkeys);
   }
   
   /// Adds ping to the request, see https://redis.io/commands/hlen
   void hlen(std::string_view key)
   {
      resp::assemble(payload, "HLEN", {key});
      cmds.push(commands::hlen);
   }

   /// Adds ping to the request, see https://redis.io/commands/hgetall
   void hgetall(std::string_view key)
   {
      resp::assemble(payload, "HGETALL", {key});
      cmds.push(commands::hgetall);
   }

   /// Adds ping to the request, see https://redis.io/commands/hvals
   void hvals( std::string_view key)
   {
      resp::assemble(payload, "HVALS", {key});
      cmds.push(commands::hvals);
   }
   
   /// Adds ping to the request, see https://redis.io/commands/hget
   void hget(std::string_view key, std::string_view field)
   {
      auto par = {field};
      resp::assemble(payload, "HGET", {key}, std::cbegin(par), std::cend(par));
      cmds.push(commands::hget);
   }
   
   /// Adds ping to the request, see https://redis.io/commands/hmget
   void hmget(
      std::string_view key,
      std::initializer_list<std::string_view> fields)
   {
      resp::assemble( payload
   	            , "HMGET"
   		    , {key}
   		    , std::cbegin(fields)
   		    , std::cend(fields));

      cmds.push(commands::hmget);
   }
   
   /// Adds ping to the request, see https://redis.io/commands/hdel
   void
   hdel(std::string_view key,
        std::initializer_list<std::string_view> fields)
   {
      resp::assemble(
	 payload,
	 "HDEL",
	 {key},
	 std::cbegin(fields),
	 std::cend(fields));

      cmds.push(commands::hdel);
   }

   /// Adds ping to the request, see https://redis.io/commands/expire
   void expire(std::string_view key, int secs)
   {
      auto const str = std::to_string(secs);
      std::initializer_list<std::string_view> par {str};
      resp::assemble(payload, "EXPIRE", {key}, std::cbegin(par), std::cend(par));
      cmds.push(commands::expire);
   }
   
   /// Adds ping to the request, see https://redis.io/commands/zadd
   void zadd(std::string_view key, int score, std::string_view value)
   {
      auto const score_str = std::to_string(score);
      std::initializer_list<std::string_view> par = {score_str, value};
      resp::assemble(payload, "ZADD", {key}, std::cbegin(par), std::cend(par));
      cmds.push(commands::zadd);
   }
   
   /// Adds ping to the request, see https://redis.io/commands/zadd
   template <class Range>
   void zadd(std::initializer_list<std::string_view> key, Range const& r)
   {
      resp::assemble(payload, "ZADD", key, std::cbegin(r), std::cend(r), 2);
      cmds.push(commands::zadd);
   }
   
   /// Adds ping to the request, see https://redis.io/commands/zrange
   void zrange(std::string_view key, int min = 0, int max = -1)
   {
      auto const min_str = std::to_string(min);
      auto const max_str = std::to_string(max);
      std::initializer_list<std::string_view> par {min_str, max_str};

      resp::assemble(payload, "ZRANGE", {key}, std::cbegin(par), std::cend(par));
      cmds.push(commands::zrange);
   }
   
   /// Adds ping to the request, see https://redis.io/commands/zrangebyscore
   void zrangebyscore(std::string_view key, int min, int max)
   {
      auto max_str = std::string {"inf"};
      if (max != -1)
         max_str = std::to_string(max);
   
      auto const min_str = std::to_string(min);
      auto par = {min_str , max_str};
      resp::assemble(payload, "ZRANGEBYSCORE", {key}, std::cbegin(par), std::cend(par));
      cmds.push(commands::zrangebyscore);
   }
   
   /// Adds ping to the request, see https://redis.io/commands/zremrangebyscore
   void
   zremrangebyscore(
      std::string_view key,
      std::string_view min,
      std::string_view max)
   {
      auto par = {min, max};
      resp::assemble(payload, "ZREMRANGEBYSCORE", {key}, std::cbegin(par), std::cend(par));
      cmds.push(commands::zremrangebyscore);
   }
   
   /// Adds ping to the request, see https://redis.io/commands/lrange
   void lrange(std::string_view key, int min = 0, int max = -1)
   {
      auto const min_str = std::to_string(min);
      auto const max_str = std::to_string(max);
      std::initializer_list<std::string_view> par {min_str, max_str};
      resp::assemble(payload, "LRANGE", {key}, std::cbegin(par), std::cend(par));
      cmds.push(commands::lrange);
   }
   
   /// Adds ping to the request, see https://redis.io/commands/ltrim
   void ltrim(std::string_view key, int min = 0, int max = -1)
   {
      auto const min_str = std::to_string(min);
      auto const max_str = std::to_string(max);
      std::initializer_list<std::string_view> par {min_str, max_str};
      resp::assemble(payload, "LTRIM", {key}, std::cbegin(par), std::cend(par));
      cmds.push(commands::ltrim);
   }
   
   // TODO: Overload for vector del.
   /// Adds ping to the request, see https://redis.io/commands/del
   void del(std::string_view key)
   {
      resp::assemble(payload, "DEL", key);
      cmds.push(commands::del);
   }
   
   /// Adds ping to the request, see https://redis.io/commands/llen
   void llen(std::string_view key)
   {
      resp::assemble(payload, "LLEN", key);
      cmds.push(commands::llen);
   }

   /// Adds ping to the request, see https://redis.io/commands/sadd
   template <class Iter>
   void sadd(std::string_view key, Iter begin, Iter end)
   {
      resp::assemble(payload, "SADD", {key}, begin, end);
      cmds.push(commands::sadd);
   }

   /// Adds ping to the request, see https://redis.io/commands/sadd
   template <class Range>
   void sadd(std::string_view key, Range const& r)
   {
      using std::cbegin;
      using std::cend;
      sadd(key, cbegin(r), cend(r));
   }

   /// Adds ping to the request, see https://redis.io/commands/smembers
   void smembers(std::string_view key)
   {
      resp::assemble(payload, "SMEMBERS", key);
      cmds.push(commands::smembers);
   }

   /// Adds ping to the request, see https://redis.io/commands/scard
   void scard(std::string_view key)
   {
      resp::assemble(payload, "SCARD", key);
      cmds.push(commands::scard);
   }

   /// Adds ping to the request, see https://redis.io/commands/scard
   void scard(std::string_view key, std::initializer_list<std::string_view> l)
   {
      resp::assemble(payload, "SDIFF", {key}, std::cbegin(l), std::cend(l));
      cmds.push(commands::scard);
   }

   /// Adds ping to the request, see https://redis.io/commands/client_id
   void client_id(std::string_view parameters)
   {
      resp::assemble(payload, "CLIENT ID", {parameters});
      cmds.push(commands::client_id);
   }
};

} // aedis
