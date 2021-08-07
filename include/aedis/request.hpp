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

#include <aedis/detail/command.hpp>

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

/** A class that generates RESP3-commands.
*/
class request {
public:
   std::string payload;
   std::queue<command> cmds;

public:
   bool empty() const noexcept { return std::empty(payload); };

   /// Clear the commands.
   void clear()
   {
      payload.clear();
      cmds = {};
   }

   /// Adds the ping command to the request. See https://redis.io/commands/ping.
   void ping()
   {
      resp::assemble(payload, "PING");
      cmds.push(command::ping);
   }

   /// Adds the quit command to the request. See https://redis.io/commands/quit
   void quit()
   {
      resp::assemble(payload, "QUIT");
      cmds.push(command::quit);
   }

   /// Adds the multi command to the request. See https://redis.io/commands/multi
   void multi()
   {
      resp::assemble(payload, "MULTI");
      cmds.push(command::multi);
   }

   /// Adds the exec command to the request. See https://redis.io/commands/exec
   void exec()
   {
      resp::assemble(payload, "EXEC");
      cmds.push(command::exec);
   }

   /// Adds the incr to the request. See 
   void incr(std::string_view key)
   {
      resp::assemble(payload, "INCR", key);
      cmds.push(command::incr);
   }

   /// Adds the auth command to the request. See
   void auth(std::string_view pwd)
   {
      resp::assemble(payload, "AUTH", pwd);
      cmds.push(command::auth);
   }

   auto bgrewriteaof()
   {
      resp::assemble(payload, "BGREWRITEAOF");
      cmds.push(command::bgrewriteaof);
   }

   auto role()
   {
      resp::assemble(payload, "ROLE");
      cmds.push(command::role);
   }

   auto bgsave()
   {
      resp::assemble(payload, "BGSAVE");
      cmds.push(command::bgsave);
   }

   auto flushall()
   {
      resp::assemble(payload, "FLUSHALL");
      cmds.push(command::flushall);
   }

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

      cmds.push(command::lpop);
   }

   void subscribe(std::string_view key)
   {
      // The response to this command is a push.
      resp::assemble(payload, "SUBSCRIBE", key);
   }

   void
   unsubscribe(std::string_view key)
   {
      // The response to this command is a push.
      resp::assemble(payload, "UNSUBSCRIBE", key);
   }

   void get(std::string_view key)
   {
      resp::assemble(payload, "GET", key);
      cmds.push(command::get);
   }

   void keys(std::string_view pattern)
   {
      resp::assemble(payload, "KEYS", pattern);
      cmds.push(command::keys);
   }

   void hello(std::string_view version = "3")
   {
      resp::assemble(payload, "HELLO", version);
      cmds.push(command::hello);
   }
   
   void sentinel(std::string_view arg, std::string_view name)
   {
      auto par = {name};
      resp::assemble(payload, "SENTINEL", {arg}, std::cbegin(par), std::cend(par));
      cmds.push(command::sentinel);
   }
   
   auto append(std::string_view key, std::string_view msg)
   {
      auto par = {msg};
      resp::assemble(payload, "APPEND", {key}, std::cbegin(par), std::cend(par));
      cmds.push(command::append);
   }
   
   auto bitcount(std::string_view key, int start = 0, int end = -1)
   {
      auto const start_str = std::to_string(start);
      auto const end_str = std::to_string(end);
      std::initializer_list<std::string_view> par {start_str, end_str};

      resp::assemble( payload
   	            , "BITCOUNT"
   		    , {key}
   		    , std::cbegin(par)
   		    , std::cend(par));
      cmds.push(command::bitcount);
   }
   
   template <class Iter>
   auto rpush(std::string_view key, Iter begin, Iter end)
   {
      resp::assemble(payload, "RPUSH", {key}, begin, end);
      cmds.push(command::rpush);
   }
   
   template <class T>
   auto rpush( std::string_view key, std::initializer_list<T> v)
   {
      return rpush(key, std::cbegin(v), std::cend(v));
   }

   template <class Range>
   void rpush( std::string_view key, Range const& v)
   {
      using std::cbegin;
      using std::cend;
      rpush(key, cbegin(v), cend(v));
   }
   
   template <class Iter>
   auto lpush(std::string_view key, Iter begin, Iter end)
   {
      resp::assemble(payload, "LPUSH", {key}, begin, end);
      cmds.push(command::lpush);
   }
   
   auto psubscribe( std::initializer_list<std::string_view> l)
   {
      std::initializer_list<std::string_view> dummy = {};
      resp::assemble(payload, "PSUBSCRIBE", l, std::cbegin(dummy), std::cend(dummy));
   }
   
   auto publish(std::string_view key, std::string_view msg)
   {
      auto par = {msg};
      resp::assemble(payload, "PUBLISH", {key}, std::cbegin(par), std::cend(par));
      cmds.push(command::publish);
   }
   
   auto set(std::string_view key,
            std::initializer_list<std::string_view> args)
   {
      resp::assemble(payload, "SET", {key}, std::cbegin(args), std::cend(args));
      cmds.push(command::set);
   }

   // TODO: Find a way to assert the value type is a pair.
   template <class Range>
   auto hset(std::string_view key, Range const& r)
   {
      //Note: Requires an std::pair as value type, otherwise gets
      //error: ERR Protocol error: expected '$', got '*'
      using std::cbegin;
      using std::cend;
      resp::assemble(payload, "HSET", {key}, std::cbegin(r), std::cend(r), 2);
      cmds.push(command::hset);
   }
   
   auto hincrby(std::string_view key, std::string_view field, int by)
   {
      auto by_str = std::to_string(by);
      std::initializer_list<std::string_view> par {field, by_str};
      resp::assemble(payload, "HINCRBY", {key}, std::cbegin(par), std::cend(par));
      cmds.push(command::hincrby);
   }
   
   auto hkeys(std::string_view key)
   {
      auto par = {""};
      resp::assemble(payload, "HKEYS", {key}, std::cbegin(par), std::cend(par));
      cmds.push(command::hkeys);
   }
   
   auto hlen(std::string_view key)
   {
      resp::assemble(payload, "HLEN", {key});
      cmds.push(command::hlen);
   }

   auto hgetall(std::string_view key)
   {
      resp::assemble(payload, "HGETALL", {key});
      cmds.push(command::hgetall);
   }

   auto hvals( std::string_view key)
   {
      resp::assemble(payload, "HVALS", {key});
      cmds.push(command::hvals);
   }
   
   auto hget(std::string_view key, std::string_view field)
   {
      auto par = {field};
      resp::assemble(payload, "HGET", {key}, std::cbegin(par), std::cend(par));
      cmds.push(command::hget);
   }
   
   auto hmget(
      std::string_view key,
      std::initializer_list<std::string_view> fields)
   {
      resp::assemble( payload
   	            , "HMGET"
   		    , {key}
   		    , std::cbegin(fields)
   		    , std::cend(fields));

      cmds.push(command::hmget);
   }
   
   auto
   hdel(std::string_view key,
        std::initializer_list<std::string_view> fields)
   {
      resp::assemble(
	 payload,
	 "HDEL",
	 {key},
	 std::cbegin(fields),
	 std::cend(fields));

      cmds.push(command::hdel);
   }

   auto expire(std::string_view key, int secs)
   {
      auto const str = std::to_string(secs);
      std::initializer_list<std::string_view> par {str};
      resp::assemble(payload, "EXPIRE", {key}, std::cbegin(par), std::cend(par));
      cmds.push(command::expire);
   }
   
   auto zadd(std::string_view key, int score, std::string_view value)
   {
      auto const score_str = std::to_string(score);
      std::initializer_list<std::string_view> par = {score_str, value};
      resp::assemble(payload, "ZADD", {key}, std::cbegin(par), std::cend(par));
      cmds.push(command::zadd);
   }
   
   template <class Range>
   auto zadd(std::initializer_list<std::string_view> key, Range const& r)
   {
      resp::assemble(payload, "ZADD", key, std::cbegin(r), std::cend(r), 2);
      cmds.push(command::zadd);
   }
   
   auto zrange(std::string_view key, int min = 0, int max = -1)
   {
      auto const min_str = std::to_string(min);
      auto const max_str = std::to_string(max);
      std::initializer_list<std::string_view> par {min_str, max_str};

      resp::assemble(payload, "ZRANGE", {key}, std::cbegin(par), std::cend(par));
      cmds.push(command::zrange);
   }
   
   auto
   zrangebyscore(std::string_view key, int min, int max)
   {
      auto max_str = std::string {"inf"};
      if (max != -1)
         max_str = std::to_string(max);
   
      auto const min_str = std::to_string(min);
      auto par = {min_str , max_str};
      resp::assemble(payload, "ZRANGEBYSCORE", {key}, std::cbegin(par), std::cend(par));
      cmds.push(command::zrangebyscore);
   }
   
   auto
   zremrangebyscore(
      std::string_view key,
      std::string_view min,
      std::string_view max)
   {
      auto par = {min, max};
      resp::assemble(payload, "ZREMRANGEBYSCORE", {key}, std::cbegin(par), std::cend(par));
      cmds.push(command::zremrangebyscore);
   }
   
   auto lrange(std::string_view key, int min = 0, int max = -1)
   {
      auto const min_str = std::to_string(min);
      auto const max_str = std::to_string(max);
      std::initializer_list<std::string_view> par {min_str, max_str};
      resp::assemble(payload, "LRANGE", {key}, std::cbegin(par), std::cend(par));
      cmds.push(command::lrange);
   }
   
   auto ltrim(std::string_view key, int min = 0, int max = -1)
   {
      auto const min_str = std::to_string(min);
      auto const max_str = std::to_string(max);
      std::initializer_list<std::string_view> par {min_str, max_str};
      resp::assemble(payload, "LTRIM", {key}, std::cbegin(par), std::cend(par));
      cmds.push(command::ltrim);
   }
   
   // TODO: Overload for vector del.
   auto del(std::string_view key)
   {
      resp::assemble(payload, "DEL", key);
      cmds.push(command::del);
   }
   
   auto llen(std::string_view key)
   {
      resp::assemble(payload, "LLEN", key);
      cmds.push(command::llen);
   }

   template <class Iter>
   void sadd(std::string_view key, Iter begin, Iter end)
   {
      resp::assemble(payload, "SADD", {key}, begin, end);
      cmds.push(command::sadd);
   }

   template <class Range>
   void sadd(std::string_view key, Range const& r)
   {
      using std::cbegin;
      using std::cend;
      sadd(key, cbegin(r), cend(r));
   }

   auto smembers(std::string_view key)
   {
      resp::assemble(payload, "SMEMBERS", key);
      cmds.push(command::smembers);
   }

   auto scard(std::string_view key)
   {
      resp::assemble(payload, "SCARD", key);
      cmds.push(command::scard);
   }

   auto scard(std::string_view key, std::initializer_list<std::string_view> l)
   {
      resp::assemble(payload, "SDIFF", {key}, std::cbegin(l), std::cend(l));
      cmds.push(command::scard);
   }

   auto client_id(std::string_view parameters)
   {
      resp::assemble(payload, "CLIENT ID", {parameters});
      cmds.push(command::client_id);
   }
};

} // aedis
