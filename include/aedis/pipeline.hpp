/* Copyright (c) 2019 - 2020 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <queue>
#include <string>
#include <algorithm>
#include <functional>
#include <type_traits>
#include <string_view>

#include <boost/asio.hpp>

namespace aedis { namespace resp {

inline
void make_bulky_item(std::string& to, std::string const& param)
{
   to += "$";
   to += std::to_string(std::size(param));
   to += "\r\n";
   to += param;
   to += "\r\n";
}

inline
void make_header(std::string& to, int size)
{
   to += "*";
   to += std::to_string(size);
   to += "\r\n";
}

struct accumulator {
   auto operator()(std::string a, std::string b) const
   {
      make_bulky_item(a, b);
      return a;
   }

   auto operator()(std::string a, int b) const
   {
      make_bulky_item(a, std::to_string(b));
      return a;
   }

   auto operator()(std::string a, std::pair<std::string, std::string> b) const
   {
      make_bulky_item(a, b.first);
      make_bulky_item(a, b.second);
      return a;
   }

   auto operator()(std::string a, std::pair<int, std::string> b) const
   {
      make_bulky_item(a, std::to_string(b.first));
      make_bulky_item(a, b.second);
      return a;
   }
};

inline
void assemble(std::string& ret, char const* cmd)
{
   make_header(ret, 1);
   make_bulky_item(ret, cmd);
}

template <class Iter>
auto assemble( std::string& ret
             , char const* cmd
             , std::initializer_list<std::string> key
             , Iter begin
             , Iter end
             , int size = 1)
{
   auto const d1 =
      std::distance( std::cbegin(key)
                   , std::cend(key));

   auto const d2 = std::distance(begin, end);

   // Perhaps, we would avoid some copying by passing ret to the
   // functions below instead of declaring a below.
   std::string a;
   make_header(a, 1 + d1 + size * d2);
   make_bulky_item(a, cmd);

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
void assemble(std::string& ret, char const* cmd, std::string const& key)
{
   std::initializer_list<std::string> dummy;
   assemble(ret, cmd, {key}, std::cbegin(dummy), std::cend(dummy));
}

enum class command {ignore};

template <class Event = command>
struct pipeline {
   std::string payload;
   std::queue<Event> events;

public:
   void ping(Event e = Event::ignore)
   {
      resp::assemble(payload, "PING");
      events.push(e);
   }

   void quit(Event e = Event::ignore)
   {
      resp::assemble(payload, "QUIT");
      events.push(e);
   }

   void multi(Event e = Event::ignore)
   {
      resp::assemble(payload, "MULTI");
      events.push(e);
   }

   void exec(Event e = Event::ignore)
   {
      resp::assemble(payload, "EXEC");
      events.push(e);
   }

   void incr(std::string const& key, Event e = Event::ignore)
   {
      resp::assemble(payload, "INCR", key);
      events.push(e);
   }

   auto auth(std::string const& pwd, Event e = Event::ignore)
   {
      resp::assemble(payload, "AUTH", pwd);
      events.push(e);
   }

   auto bgrewriteaof(Event e = Event::ignore)
   {
      resp::assemble(payload, "BGREWRITEAOF");
      events.push(e);
   }

   auto role(Event e = Event::ignore)
   {
      resp::assemble(payload, "ROLE");
      events.push(e);
   }

   auto bgsave(Event e = Event::ignore)
   {
      resp::assemble(payload, "BGSAVE");
   }

   auto flushall(Event e = Event::ignore)
   {
      resp::assemble(payload, "FLUSHALL");
      events.push(e);
   }

   auto lpop(std::string const& key, Event e = Event::ignore)
   {
      resp::assemble(payload, "LPOP", key);
      events.push(e);
   }

   auto subscribe(std::string const& key, Event e = Event::ignore)
   {
      resp::assemble(payload, "SUBSCRIBE", key);
      events.push(e);
   }

   auto unsubscribe(std::string const& key, Event e = Event::ignore)
   {
      resp::assemble(payload, "UNSUBSCRIBE", key);
      events.push(e);
   }

   auto get(std::string const& key, Event e = Event::ignore)
   {
      resp::assemble(payload, "GET", key);
      events.push(e);
   }

   void hello(std::string const& key, Event e = Event::ignore)
   {
      resp::assemble(payload, "HELLO", key);
      events.push(e);
   }
   
   auto sentinel(std::string const& arg, std::string const& name, Event e = Event::ignore)
   {
      auto par = {name};
      resp::assemble(payload, "SENTINEL", {arg}, std::cbegin(par), std::cend(par));
      events.push(e);
   }
   
   auto append(std::string const& key, std::string const& msg, Event e = Event::ignore)
   {
      auto par = {msg};
      resp::assemble(payload, "APPEND", {key}, std::cbegin(par), std::cend(par));
      events.push(e);
   }
   
   auto bitcount(std::string const& key, int start = 0, int end = -1, Event e = Event::ignore)
   {
      auto par = {std::to_string(start), std::to_string(end)};
      resp::assemble( payload
   	            , "BITCOUNT"
   		    , {key}
   		    , std::cbegin(par)
   		    , std::cend(par));
      events.push(e);
   }
   
   template <class Iter>
   auto rpush(std::string const& key, Iter begin, Iter end, Event e = Event::ignore)
   {
      resp::assemble(payload, "RPUSH", {key}, begin, end);
      events.push(e);
   }
   
   template <class T>
   auto rpush(std::string const& key, std::initializer_list<T> v, Event e = Event::ignore)
   {
      return rpush(key, std::cbegin(v), std::cend(v), e);
   }

   template <class Range>
   void rpush(std::string const& key, Range const& v, Event e = Event::ignore)
   {
      using std::cbegin;
      using std::cend;
      rpush(key, cbegin(v), cend(v), e);
   }
   
   template <class Iter>
   auto lpush(std::string const& key, Iter begin, Iter end, Event e = Event::ignore)
   {
      resp::assemble(payload, "LPUSH", {key}, begin, end);
      events.push(e);
   }
   
   auto psubscribe(std::initializer_list<std::string> l, Event e = Event::ignore)
   {
      std::initializer_list<std::string> dummy = {};
      resp::assemble(payload, "PSUBSCRIBE", l, std::cbegin(dummy), std::cend(dummy));
      events.push(e);
   }
   
   auto publish(std::string const& key, std::string const& msg, Event e = Event::ignore)
   {
      auto par = {msg};
      resp::assemble(payload, "PUBLISH", {key}, std::cbegin(par), std::cend(par));
      events.push(e);
   }
   
   auto
   set(std::string const& key,
       std::initializer_list<std::string> args,
       Event e = Event::ignore)
   {
      resp::assemble(payload, "SET", {key}, std::cbegin(args), std::cend(args));
   }

   auto
   hset(std::string const& key,
	std::initializer_list<std::string> l,
        Event e = Event::ignore)
   {
      resp::assemble(payload, "HSET", {key}, std::cbegin(l), std::cend(l));
      events.push(e);
   }

   // TODO: Remove this, use ranges instead.
   template <class Key, class T, class Compare, class Allocator>
   auto
   hset(std::string const& key,
	std::map<Key, T, Compare, Allocator> const& m,
        Event e = Event::ignore)
   {
      resp::assemble(payload, "HSET", {key}, std::cbegin(m), std::cend(m), 2);
      events.push(e);
   }
   
   auto
   hincrby(std::string const& key,
	   std::string const& field,
	   int by,
	   Event e = Event::ignore)
   {
      auto par = {field, std::to_string(by)};
      resp::assemble(payload, "HINCRBY", {key}, std::cbegin(par), std::cend(par));
      events.push(e);
   }
   
   auto
   hkeys(std::string const& key, Event e = Event::ignore)
   {
      auto par = {""};
      resp::assemble(payload, "HKEYS", {key}, std::cbegin(par), std::cend(par));
      events.push(e);
   }
   
   auto hlen(std::string const& key, Event e = Event::ignore)
   {
      resp::assemble(payload, "HLEN", {key});
      events.push(e);
   }

   auto hgetall(std::string const& key, Event e = Event::ignore)
   {
      resp::assemble(payload, "HGETALL", {key});
      events.push(e);
   }

   auto hvals(std::string const& key, Event e = Event::ignore)
   {
      resp::assemble(payload, "HVALS", {key});
      events.push(e);
   }
   
   auto hget(std::string const& key, std::string const& field, Event e = Event::ignore)
   {
      auto par = {field};
      resp::assemble(payload, "HGET", {key}, std::cbegin(par), std::cend(par));
      events.push(e);
   }
   
   auto
   hmget(std::string const& key,
	 std::initializer_list<std::string> fields,
	 Event e = Event::ignore)
   {
      resp::assemble( payload
   	         , "HMGET"
   		 , {key}
   		 , std::cbegin(fields)
   		 , std::cend(fields));
      events.push(e);
   }
   
   auto expire(std::string const& key, int secs, Event e = Event::ignore)
   {
      auto par = {std::to_string(secs)};
      resp::assemble(payload, "EXPIRE", {key}, std::cbegin(par), std::cend(par));
      events.push(e);
   }
   
   auto
   zadd(std::string const& key,
	int score, std::string const& value,
	Event e = Event::ignore)
   {
      auto par = {std::to_string(score), value};
      resp::assemble(payload, "ZADD", {key}, std::cbegin(par), std::cend(par));
      events.push(e);
   }
   
   // TODO: Remove this overload and use ranges.
   template <class Key, class T, class Compare, class Allocator>
   auto
   zadd(std::initializer_list<std::string> key,
	std::map<Key, T, Compare, Allocator> const& m,
	Event e = Event::ignore)
   {
      resp::assemble(payload, "ZADD", key, std::cbegin(m), std::cend(m), 2);
      events.push(e);
   }
   
   auto
   zrange(std::string const& key,
	  int min = 0,
	  int max = -1,
	  Event e = Event::ignore)
   {
      auto par = {std::to_string(min), std::to_string(max)};
      resp::assemble(payload, "ZRANGE", {key}, std::cbegin(par), std::cend(par));
      events.push(e);
   }
   
   auto
   zrangebyscore(std::string const& key,
	         int min,
		 int max,
		 Event e = Event::ignore)
   {
      auto max_str = std::string {"inf"};
      if (max != -1)
         max_str = std::to_string(max);
   
      auto par = {std::to_string(min) , max_str};
      resp::assemble(payload, "ZRANGEBYSCORE", {key}, std::cbegin(par), std::cend(par));
      events.push(e);
   }
   
   auto zremrangebyscore(std::string const& key, int score)
   {
      auto const s = std::to_string(score);
      auto par = {s, s};
      resp::assemble(payload, "ZREMRANGEBYSCORE", {key}, std::cbegin(par), std::cend(par));
   }
   
   auto lrange(std::string const& key, int min = 0, int max = -1, Event e = Event::ignore)
   {
      auto par = { std::to_string(min) , std::to_string(max) };
      resp::assemble(payload, "LRANGE", {key}, std::cbegin(par), std::cend(par));
      events.push(e);
   }
   
   auto ltrim(std::string const& key, int min = 0, int max = -1, Event e = Event::ignore)
   {
      auto par = { std::to_string(min) , std::to_string(max) };
      resp::assemble(payload, "LTRIM", {key}, std::cbegin(par), std::cend(par));
      events.push(e);
   }
   
   auto del(std::string const& key, Event e = Event::ignore)
   {
      resp::assemble(payload, "DEL", key);
      events.push(e);
   }
   
   auto llen(std::string const& key, Event e = Event::ignore)
   {
      resp::assemble(payload, "LLEN", key);
      events.push(e);
   }

   template <class Iter>
   void sadd(std::string const& key, Iter begin, Iter end, Event e = Event::ignore)
   {
      resp::assemble(payload, "SADD", {key}, begin, end);
      events.push(e);
   }

   template <class Range>
   void sadd(std::string const& key, Range const& r, Event e = Event::ignore)
   {
      using std::cbegin;
      using std::cend;
      sadd(key, cbegin(r), cend(r), e);
   }

   auto smembers(std::string const& key, Event e = Event::ignore)
   {
      resp::assemble(payload, "SMEMBERS", key);
      events.push(e);
   }

   auto scard(std::string const& key, Event e = Event::ignore)
   {
      resp::assemble(payload, "SCARD", key);
      events.push(e);
   }

   auto
   scard(std::string const& key,
	 std::initializer_list<std::string> l,
	 Event e = Event::ignore)
   {
      resp::assemble(payload, "SDIFF", {key}, std::cbegin(l), std::cend(l));
      events.push(e);
   }
};

} // resp
} // aedis
