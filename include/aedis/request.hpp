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
#include <algorithm>
#include <functional>
#include <type_traits>
#include <string_view>

#include <boost/asio.hpp>
#include <boost/beast/core/stream_traits.hpp>

#include "command.hpp"

namespace aedis { namespace resp {

inline
void make_bulk(std::string& to, std::string_view param)
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
   auto
   operator()(
      std::string a,
      std::string_view b) const
   {
      make_bulk(a, b);
      return a;
   }

   template <class T>
   auto
   operator()(
      std::string a,
      T b,
      std::enable_if<(std::is_integral<T>::value || std::is_floating_point<T>::value),
		      bool>::type = false) const
   {
      make_bulk(a, std::to_string(b));
      return a;
   }

   auto
   operator()(
      std::string a,
      std::pair<std::string, std::string_view> b) const
   {
      make_bulk(a, b.first);
      make_bulk(a, b.second);
      return a;
   }

   template <class T>
   auto
   operator()(
      std::string a,
      std::pair<T, std::string_view> b,
      std::enable_if<(std::is_integral<T>::value || std::is_floating_point<T>::value),
		      bool>::type = false) const
   {
      make_bulk(a, std::to_string(b.first));
      make_bulk(a, b.second);
      return a;
   }
};

inline
void assemble(std::string& ret, std::string_view cmd)
{
   make_header(ret, 1);
   make_bulk(ret, cmd);
}

template <class Iter>
auto assemble( std::string& ret
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

   // Perhaps, we would avoid some copying by passing ret to the
   // functions below instead of declaring a below.
   std::string a;
   make_header(a, 1 + d1 + size * d2);
   make_bulk(a, cmd);

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

enum class event {ignore};

// TODO: Make the write functions friend of this class and make the
// payload private.
template <class Event = event>
class request {
public:
   std::string payload;
   std::queue<std::pair<command, Event>> events;

public:
   bool empty() const noexcept { return std::empty(payload); };
   void clear()
   {
      payload.clear();
      events = {};
   }

   void ping(Event e = Event::ignore)
   {
      resp::assemble(payload, "PING");
      events.push({command::ping, e});
   }

   void quit(Event e = Event::ignore)
   {
      resp::assemble(payload, "QUIT");
      events.push({command::quit, e});
   }

   void multi(Event e = Event::ignore)
   {
      resp::assemble(payload, "MULTI");
      events.push({command::multi, e});
   }

   void exec(Event e = Event::ignore)
   {
      resp::assemble(payload, "EXEC");
      events.push({command::exec, e});
   }

   void incr(std::string_view key, Event e = Event::ignore)
   {
      resp::assemble(payload, "INCR", key);
      events.push({command::incr, e});
   }

   void
   auth(
      std::string_view pwd,
      Event e = Event::ignore)
   {
      resp::assemble(payload, "AUTH", pwd);
      events.push({command::auth, e});
   }

   auto bgrewriteaof(Event e = Event::ignore)
   {
      resp::assemble(payload, "BGREWRITEAOF");
      events.push({command::bgrewriteaof, e});
   }

   auto role(Event e = Event::ignore)
   {
      resp::assemble(payload, "ROLE");
      events.push({command::role, e});
   }

   auto bgsave(Event e = Event::ignore)
   {
      resp::assemble(payload, "BGSAVE");
      events.push({command::bgsave, e});
   }

   auto flushall(Event e = Event::ignore)
   {
      resp::assemble(payload, "FLUSHALL");
      events.push({command::flushall, e});
   }

   void
   lpop(
      std::string_view key,
      Event e = Event::ignore)
   {
      resp::assemble(payload, "LPOP", key);
      events.push({command::lpop, e});
   }

   void
   subscribe(
      std::string_view key,
      Event e = Event::ignore)
   {
      resp::assemble(payload, "SUBSCRIBE", key);
      // It looks like in resp3 there is not response for subscribe. 
      //events.push({command::subscribe, e});
   }

   void
   unsubscribe(
      std::string_view key,
      Event e = Event::ignore)
   {
      resp::assemble(payload, "UNSUBSCRIBE", key);
      events.push({command::unsubscribe, e});
   }

   void
   get(
      std::string_view key,
      Event e = Event::ignore)
   {
      resp::assemble(payload, "GET", key);
      events.push({command::get, e});
   }

   void
   keys(
      std::string_view pattern,
      Event e = Event::ignore)
   {
      resp::assemble(payload, "KEYS", pattern);
      events.push({command::keys, e});
   }

   void
   hello(
      std::string_view version = "3",
      Event e = Event::ignore)
   {
      resp::assemble(payload, "HELLO", version);
      events.push({command::hello, e});
   }
   
   void
   sentinel(
      std::string_view arg,
      std::string_view name,
      Event e = Event::ignore)
   {
      auto par = {name};
      resp::assemble(payload, "SENTINEL", {arg}, std::cbegin(par), std::cend(par));
      events.push({command::sentinel, e});
   }
   
   auto
   append(
      std::string_view key,
      std::string_view msg,
      Event e = Event::ignore)
   {
      auto par = {msg};
      resp::assemble(payload, "APPEND", {key}, std::cbegin(par), std::cend(par));
      events.push({command::append, e});
   }
   
   auto
   bitcount(
      std::string_view key,
      int start = 0,
      int end = -1,
      Event e = Event::ignore)
   {
      auto par = {std::to_string(start), std::to_string(end)};
      resp::assemble( payload
   	            , "BITCOUNT"
   		    , {key}
   		    , std::cbegin(par)
   		    , std::cend(par));
      events.push({command::bitcount, e});
   }
   
   template <class Iter>
   auto
   rpush(
      std::string_view key,
      Iter begin,
      Iter end,
      Event e = Event::ignore)
   {
      resp::assemble(payload, "RPUSH", {key}, begin, end);
      events.push({command::rpush, e});
   }
   
   template <class T>
   auto
   rpush(
      std::string_view key,
      std::initializer_list<T> v,
      Event e = Event::ignore)
   {
      return rpush(key, std::cbegin(v), std::cend(v), e);
   }

   template <class Range>
   void
   rpush(
      std::string_view key,
      Range const& v, Event e = Event::ignore)
   {
      using std::cbegin;
      using std::cend;
      rpush(key, cbegin(v), cend(v), e);
   }
   
   template <class Iter>
   auto
   lpush(
      std::string_view key,
      Iter begin,
      Iter end,
      Event e = Event::ignore)
   {
      resp::assemble(payload, "LPUSH", {key}, begin, end);
      events.push({command::lpush, e});
   }
   
   auto
   psubscribe(
      std::initializer_list<std::string_view> l,
      Event e = Event::ignore)
   {
      std::initializer_list<std::string_view> dummy = {};
      resp::assemble(payload, "PSUBSCRIBE", l, std::cbegin(dummy), std::cend(dummy));
      events.push({command::psubscribe, e});
   }
   
   auto
   publish(
      std::string_view key,
      std::string_view msg,
      Event e = Event::ignore)
   {
      auto par = {msg};
      resp::assemble(payload, "PUBLISH", {key}, std::cbegin(par), std::cend(par));
      events.push({command::publish, e});
   }
   
   auto
   set(std::string_view key,
       std::initializer_list<std::string_view> args,
       Event e = Event::ignore)
   {
      resp::assemble(payload, "SET", {key}, std::cbegin(args), std::cend(args));
      events.push({command::set, e});
   }

   // TODO: Find a way to assert the value type is a pair.
   template <class Range>
   auto
   hset(
      std::string_view key,
      Range const& r, Event e = Event::ignore)
   {
      using std::cbegin;
      using std::cend;
      resp::assemble(payload, "HSET", {key}, std::cbegin(r), std::cend(r), 2);
      events.push({command::hset, e});
   }
   
   auto
   hincrby(
      std::string_view key,
      std::string_view field,
      int by,
      Event e = Event::ignore)
   {
      auto par = {field, std::to_string(by)};
      resp::assemble(payload, "HINCRBY", {key}, std::cbegin(par), std::cend(par));
      events.push({command::hincrby, e});
   }
   
   auto
   hkeys(
      std::string_view key,
      Event e = Event::ignore)
   {
      auto par = {""};
      resp::assemble(payload, "HKEYS", {key}, std::cbegin(par), std::cend(par));
      events.push({command::hkeys, e});
   }
   
   auto
   hlen(
      std::string_view key,
      Event e = Event::ignore)
   {
      resp::assemble(payload, "HLEN", {key});
      events.push({command::hlen, e});
   }

   auto
   hgetall(
      std::string_view key,
      Event e = Event::ignore)
   {
      resp::assemble(payload, "HGETALL", {key});
      events.push({command::hgetall, e});
   }

   auto
   hvals(
      std::string_view key,
      Event e = Event::ignore)
   {
      resp::assemble(payload, "HVALS", {key});
      events.push({command::hvals, e});
   }
   
   auto
   hget(
      std::string_view key,
      std::string_view field,
      Event e = Event::ignore)
   {
      auto par = {field};
      resp::assemble(payload, "HGET", {key}, std::cbegin(par), std::cend(par));
      events.push({command::hget, e});
   }
   
   auto
   hmget(
      std::string_view key,
      std::initializer_list<std::string_view> fields,
      Event e = Event::ignore)
   {
      resp::assemble( payload
   	            , "HMGET"
   		    , {key}
   		    , std::cbegin(fields)
   		    , std::cend(fields));

      events.push({command::hmget, e});
   }
   
   auto
   expire(
      std::string_view key,
      int secs,
      Event e = Event::ignore)
   {
      auto par = {std::to_string(secs)};
      resp::assemble(payload, "EXPIRE", {key}, std::cbegin(par), std::cend(par));
      events.push({command::expire, e});
   }
   
   auto
   zadd(
      std::string_view key,
      int score,
      std::string_view value,
      Event e = Event::ignore)
   {
      std::initializer_list<std::string_view> par =
	 {std::to_string(score), value};
      resp::assemble(payload, "ZADD", {key}, std::cbegin(par), std::cend(par));
      events.push({command::zadd, e});
   }
   
   template <class Range>
   auto
   zadd(std::initializer_list<std::string_view> key,
	Range const& r,
	Event e = Event::ignore)
   {
      resp::assemble(payload, "ZADD", key, std::cbegin(r), std::cend(r), 2);
      events.push({command::zadd, e});
   }
   
   auto
   zrange(std::string_view key,
	  int min = 0,
	  int max = -1,
	  Event e = Event::ignore)
   {
      auto par = {std::to_string(min), std::to_string(max)};
      resp::assemble(payload, "ZRANGE", {key}, std::cbegin(par), std::cend(par));
      events.push({command::zrange, e});
   }
   
   auto
   zrangebyscore(
      std::string_view key,
      int min,
      int max,
      Event e = Event::ignore)
   {
      auto max_str = std::string {"inf"};
      if (max != -1)
         max_str = std::to_string(max);
   
      auto par = {std::to_string(min) , max_str};
      resp::assemble(payload, "ZRANGEBYSCORE", {key}, std::cbegin(par), std::cend(par));
      events.push({command::zrangebyscore, e});
   }
   
   auto
   zremrangebyscore(
      std::string_view key,
      int score,
      Event e = Event::ignore)
   {
      auto const s = std::to_string(score);
      auto par = {s, s};
      resp::assemble(payload, "ZREMRANGEBYSCORE", {key}, std::cbegin(par), std::cend(par));
      events.push({command::zremrangebyscore, e});
   }
   
   auto
   lrange(
      std::string_view key,
      int min = 0,
      int max = -1,
      Event e = Event::ignore)
   {
      auto par = {std::to_string(min), std::to_string(max)};
      resp::assemble(payload, "LRANGE", {key}, std::cbegin(par), std::cend(par));
      events.push({command::lrange, e});
   }
   
   auto
   ltrim(
      std::string_view key,
      int min = 0,
      int max = -1,
      Event e = Event::ignore)
   {
      auto par = {std::to_string(min), std::to_string(max)};
      resp::assemble(payload, "LTRIM", {key}, std::cbegin(par), std::cend(par));
      events.push({command::ltrim, e});
   }
   
   auto
   del(
      std::string_view key,
      Event e = Event::ignore)
   {
      resp::assemble(payload, "DEL", key);
      events.push({command::del, e});
   }
   
   auto
   llen(
      std::string_view key,
      Event e = Event::ignore)
   {
      resp::assemble(payload, "LLEN", key);
      events.push({command::llen, e});
   }

   template <class Iter>
   void
   sadd(
      std::string_view key,
      Iter begin,
      Iter end,
      Event e = Event::ignore)
   {
      resp::assemble(payload, "SADD", {key}, begin, end);
      events.push({command::sadd, e});
   }

   template <class Range>
   void
   sadd(
      std::string_view key,
      Range const& r,
      Event e = Event::ignore)
   {
      using std::cbegin;
      using std::cend;
      sadd(key, cbegin(r), cend(r), e);
   }

   auto
   smembers(
      std::string_view key,
      Event e = Event::ignore)
   {
      resp::assemble(payload, "SMEMBERS", key);
      events.push({command::smembers, e});
   }

   auto
   scard(
      std::string_view key,
      Event e = Event::ignore)
   {
      resp::assemble(payload, "SCARD", key);
      events.push({command::scard, e});
   }

   auto
   scard(
      std::string_view key,
      std::initializer_list<std::string_view> l,
      Event e = Event::ignore)
   {
      resp::assemble(payload, "SDIFF", {key}, std::cbegin(l), std::cend(l));
      events.push({command::scard, e});
   }

   auto
   client(
      std::string_view parameters,
      Event e = Event::ignore)
   {
      resp::assemble(payload, "CLIENT", {parameters});
      events.push({command::client, e});
   }
};

} // resp
} // aedis
