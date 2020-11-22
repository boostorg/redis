/* Copyright (c) 2019 - 2020 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <set>
#include <list>
#include <array>
#include <deque>
#include <queue>
#include <vector>
#include <string>
#include <cstdio>
#include <utility>
#include <cstdlib>
#include <cstring>
#include <numeric>
#include <iostream>
#include <algorithm>
#include <functional>
#include <type_traits>
#include <string_view>
#include <forward_list>
#include <unordered_set>

#include <boost/asio.hpp>

#include <fmt/format.h>
#include <fmt/ostream.h>

namespace aedis
{

namespace net = boost::asio;
namespace ip = net::ip;
using tcp = ip::tcp;

namespace resp
{

using buffer = std::string;

struct response {
   std::vector<std::string> res;

   void add(std::string_view s)
      { res.emplace_back(s.data(), std::size(s)); }

   void clear()
      { res.clear(); }

   auto size()
      { return std::size(res); }
};

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

// Converts a decimal number in ascii format to integer.
inline
std::size_t get_length(char const* p)
{
   std::size_t len = 0;
   while (*p != '\r') {
       len = (10 * len) + (*p - '0');
       p++;
   }
   return len;
}

void print(std::vector<std::string> const& v)
{
   for (auto const& o : v)
     std::cout << o << " ";
   std::cout << std::endl;
}

void print_command_raw(std::string const& data, int n)
{
  for (int i = 0; i < n; ++i) {
    if (data[i] == '\n') {
      std::cout << "\\n";
      continue;
    }
    if (data[i] == '\r') {
      std::cout << "\\r";
      continue;
    }
    std::cout << data[i];
  }
}

// The parser supports up to 5 levels of nested structures. The first
// element in the sizes stack is a sentinel and must be different from
// 1.
template <class AsyncReadStream>
struct parse_op {
   AsyncReadStream& socket;
   resp::buffer* buf = nullptr;
   resp::response* res = nullptr;
   int start = 1;
   int depth = 0;
   int sizes[6] = {2, 1, 1, 1, 1, 1};
   bool bulky = false;

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code const& ec = {}
                  , std::size_t n = 0)
   {
      switch (start) {
         for (;;) {
            case 1:
            start = 0;
            net::async_read_until( socket
                                 , net::dynamic_buffer(*buf)
                                 , "\r\n"
                                 , std::move(self));
            return; default:

            if (ec || n < 3)
               return self.complete(ec);

            auto str_flag = false;
            if (bulky) {
               res->add({&buf->front(), n - 2});
               --sizes[depth];
            } else {
               if (sizes[depth] != 0) {
                  switch (buf->front()) {
                     case '$':
                     {
                        // We may want to consider not pushing in the vector
                        // but find a way to report nil.
                        if (buf->compare(1, 2, "-1") == 0) {
                           res->add({});
                           --sizes[depth];
                        } else {
                           str_flag = true;
                        }
                     } break;
                     case '+':
                     case '-':
                     case ':':
                     {
                        res->add({&(*buf)[1], n - 3});
                        --sizes[depth];
                     } break;
                     case '*':
                     {
                        sizes[++depth] = get_length(buf->data() + 1);
                     } break;
                     default:
                        assert(false);
                  }
               }
            }

            
            //print_command_raw(*buf, n);
            buf->erase(0, n);

            while (sizes[depth] == 0)
               --sizes[--depth];

            if (depth == 0 && !str_flag) {
               //std::cout << std::endl;
               return self.complete({});
            }

            bulky = str_flag;
         }
      }
   }
};

template <
   class AsyncReadStream,
   class CompletionToken =
      net::default_completion_token_t<typename AsyncReadStream::executor_type>
   >
auto async_read(
   AsyncReadStream& stream,
   resp::buffer& buffer,
   resp::response& res,
   CompletionToken&& token =
      net::default_completion_token_t<typename AsyncReadStream::executor_type>{})
{
   return net::async_compose
      < CompletionToken
      , void(boost::system::error_code)
      >(parse_op<AsyncReadStream> {stream, &buffer, &res},
        token,
        stream);
}

struct pipeline {
   std::string payload;

public:
   void ping()
      { resp::assemble(payload, "PING"); }
   void quit()
      { resp::assemble(payload, "QUIT"); }
   void multi()
      { resp::assemble(payload, "MULTI"); }
   void exec()
      { resp::assemble(payload, "EXEC"); }
   void incr(std::string const& key)
      { resp::assemble(payload, "INCR", key); }
};

}

inline
auto sentinel(std::string const& arg, std::string const& name)
{
   std::string ret;
   auto par = {name};
   resp::assemble(ret, "SENTINEL", {arg}, std::cbegin(par), std::cend(par));
   return ret;
}

inline
auto append(std::string const& key, std::string const& msg)
{
   std::string ret;
   auto par = {msg};
   resp::assemble(ret, "APPEND", {key}, std::cbegin(par), std::cend(par));
   return ret;
}

inline
auto auth(std::string const& pwd)
{
   std::string ret;
   resp::assemble(ret, "AUTH", pwd);
   return ret;
}

inline
auto bgrewriteaof()
{
   std::string ret;
   resp::assemble(ret, "BGREWRITEAOF");
   return ret;
}

inline
auto role()
{
   std::string ret;
   resp::assemble(ret, "ROLE");
   return ret;
}

inline
auto bgsave()
{
   std::string ret;
   resp::assemble(ret, "BGSAVE");
   return ret;
}

inline
auto bitcount(std::string const& key, int start = 0, int end = -1)
{
   std::string ret;
   auto par = {std::to_string(start), std::to_string(end)};
   resp::assemble( ret
	         , "BITCOUNT"
		 , {key}
		 , std::cbegin(par)
		 , std::cend(par));
   return ret;
}

template <class Iter>
auto rpush(std::string const& key, Iter begin, Iter end)
{
   std::string ret;
   resp::assemble(ret, "RPUSH", {key}, begin, end);
   return ret;
}

template <class T, class Allocator>
auto rpush( std::string const& key
          , std::vector<T, Allocator> const& v)
{
   return rpush(key, std::cbegin(v), std::cend(v));
}

template <class T, class Allocator>
auto rpush( std::string const& key
          , std::deque<T, Allocator> const& v)
{
   return rpush(key, std::cbegin(v), std::cend(v));
}

template <class T, std::size_t N>
auto rpush( std::string const& key
          , std::array<T, N> const& a)
{
   return rpush(key, std::cbegin(a), std::cend(a));
}

template <class T, class Allocator>
auto rpush( std::string const& key
          , std::list<T, Allocator> const& l)
{
   return rpush(key, std::cbegin(l), std::cend(l));
}

template <class T, class Allocator>
auto rpush( std::string const& key
          , std::forward_list<T, Allocator> const& l)
{
   return rpush(key, std::cbegin(l), std::cend(l));
}

template <class T, class Compare, class Allocator>
auto rpush( std::string const& key
          , std::set<T, Compare, Allocator> const& s)
{
   return rpush(key, std::cbegin(s), std::cend(s));
}

template <class T, class Compare, class Allocator>
auto rpush( std::string const& key
          , std::multiset<T, Compare, Allocator> const& s)
{
   return rpush(key, std::cbegin(s), std::cend(s));
}

template <class T, class Hash, class KeyEqual, class Allocator>
auto rpush( std::string const& key
          , std::unordered_set< T, Hash, KeyEqual, Allocator
                              > const& s)
{
   return rpush(key, std::cbegin(s), std::cend(s));
}

template <class T, class Hash, class KeyEqual, class Allocator>
auto rpush( std::string const& key
          , std::unordered_multiset< T, Hash, KeyEqual, Allocator
                                   > const& s)
{
   return rpush(key, std::cbegin(s), std::cend(s));
}

template <class Iter>
auto lpush(std::string const& key, Iter begin, Iter end)
{
   std::string ret;
   resp::assemble(ret, "LPUSH", {key}, begin, end);
   return ret;
}

inline
auto quit()
{
   std::string ret;
   resp::assemble(ret, "QUIT");
   return ret;
}

inline
auto multi()
{
   std::string ret;
   resp::assemble(ret, "MULTI");
   return ret;
}

inline
auto ping()
{
   std::string ret;
   resp::assemble(ret, "PING");
   return ret;
}

inline
auto flushall()
{
   std::string ret;
   resp::assemble(ret, "FLUSHALL");
   return ret;
}

inline
auto exec()
{
   std::string ret;
   resp::assemble(ret, "EXEC");
   return ret;
}

inline
auto incr(std::string const& key)
{
   std::string ret;
   resp::assemble(ret, "INCR", key);
   return ret;
}

inline
auto lpop(std::string const& key)
{
   std::string ret;
   resp::assemble(ret, "LPOP", key);
   return ret;
}

inline
auto subscribe(std::string const& key)
{
   std::string ret;
   resp::assemble(ret, "SUBSCRIBE", key);
   return ret;
}

inline
auto psubscribe(std::initializer_list<std::string> l)
{
   std::initializer_list<std::string> dummy;

   std::string ret;
   resp::assemble(
      ret,
      "PSUBSCRIBE",
      l,
      std::cbegin(dummy),
      std::cend(dummy));
   return ret;
}

inline
auto unsubscribe(std::string const& key)
{
   std::string ret;
   resp::assemble(ret, "UNSUBSCRIBE", key);
   return ret;
}

inline
auto get(std::string const& key)
{
   std::string ret;
   resp::assemble(ret, "GET", key);
   return ret;
}

inline
auto publish(std::string const& key, std::string const& msg)
{
   auto par = {msg};
   std::string ret;
   resp::assemble(ret, "PUBLISH", {key}, std::cbegin(par), std::cend(par));
   return ret;
}

inline
auto set( std::string const& key
        , std::initializer_list<std::string> args)
{
   std::string ret;
   resp::assemble(ret, "SET", {key}, std::cbegin(args), std::cend(args));
   return ret;
}

inline
auto hset( std::string const& key
         , std::initializer_list<std::string> l)
{
   std::string ret;
   resp::assemble(ret, "HSET", {key}, std::cbegin(l), std::cend(l));
   return ret;
}

template <class Key, class T, class Compare, class Allocator>
auto hset( std::string const& key
         , std::map<Key, T, Compare, Allocator> const& m)
{
   std::string ret;
   resp::assemble(ret, "HSET", {key}, std::cbegin(m), std::cend(m), 2);
   return ret;
}

inline
auto hincrby(std::string const& key, std::string const& field, int by)
{
   auto par = {field, std::to_string(by)};
   std::string ret;
   resp::assemble(ret, "HINCRBY", {key}, std::cbegin(par), std::cend(par));
   return ret;
}

inline
auto hkeys(std::string const& key)
{
   std::initializer_list<std::string> par;
   std::string ret;
   resp::assemble(ret, "HKEYS", {key}, std::cbegin(par), std::cend(par));
   return ret;
}

inline
auto hlen(std::string const& key)
{
   std::initializer_list<std::string> par;
   std::string ret;
   resp::assemble(ret, "HLEN", {key});
   return ret;
}

inline
auto hgetall(std::string const& key)
{
   std::initializer_list<std::string> par;
   std::string ret;
   resp::assemble(ret, "HGETALL", {key});
   return ret;
}

inline
auto hvals(std::string const& key)
{
   std::string ret;
   resp::assemble(ret, "HVALS", {key});
   return ret;
}

inline
auto hget(std::string const& key, std::string const& field)
{
   auto par = {field};
   std::string ret;
   resp::assemble(ret, "HGET", {key}, std::cbegin(par), std::cend(par));
   return ret;
}

inline
auto hmget( std::string const& key
          , std::initializer_list<std::string> fields)
{
   std::string ret;
   resp::assemble( ret
	         , "HMGET"
		 , {key}
		 , std::cbegin(fields)
		 , std::cend(fields));
   return ret;
}

inline
auto expire(std::string const& key, int secs)
{
   auto par = {std::to_string(secs)};
   std::string ret;
   resp::assemble(ret, "EXPIRE", {key}, std::cbegin(par), std::cend(par));
   return ret;
}

inline
auto zadd(std::string const& key, int score, std::string const& value)
{
   auto par = {std::to_string(score), value};
   std::string ret;
   resp::assemble(ret, "ZADD", {key}, std::cbegin(par), std::cend(par));
   return ret;
}

template <class Key, class T, class Compare, class Allocator>
auto zadd( std::initializer_list<std::string> key
         , std::map<Key, T, Compare, Allocator> const& m)
{
   std::string ret;
   resp::assemble(ret, "ZADD", key, std::cbegin(m), std::cend(m), 2);
   return ret;
}

inline
auto zrange(std::string const& key, int min = 0, int max = -1)
{
   auto par = { std::to_string(min), std::to_string(max) };
   std::string ret;
   resp::assemble(ret, "ZRANGE", {key}, std::cbegin(par), std::cend(par));
   return ret;
}

inline
auto zrangebyscore(std::string const& key, int min, int max)
{
   auto max_str = std::string {"inf"};
   if (max != -1)
      max_str = std::to_string(max);

   auto par = { std::to_string(min) , max_str };
   std::string ret;
   resp::assemble(ret, "zrangebyscore", {key}, std::cbegin(par), std::cend(par));
   return ret;
}

inline
auto zremrangebyscore(std::string const& key, int score)
{
   auto const s = std::to_string(score);
   auto par = {s, s};
   std::string ret;
   resp::assemble(ret, "ZREMRANGEBYSCORE", {key}, std::cbegin(par), std::cend(par));
   return ret;
}

inline
auto lrange(std::string const& key, int min = 0, int max = -1)
{
   auto par = { std::to_string(min) , std::to_string(max) };
   std::string ret;
   resp::assemble(ret, "lrange", {key}, std::cbegin(par), std::cend(par));
   return ret;
}

inline
auto del(std::string const& key)
{
   std::string ret;
   resp::assemble(ret, "del", key);
   return ret;
}

inline
auto llen(std::string const& key)
{
   std::string ret;
   resp::assemble(ret, "llen", key);
   return ret;
}

namespace log
{

enum class level
{ emerg
, alert
, crit
, err
, warning
, notice
, info
, debug
};

template <class... Args>
void write(level filter, level ll, char const* fmt, Args const& ... args)
{
   if (ll > filter)
      return;

   std::clog << fmt::format(fmt, args...) << std::endl;
}

}

struct instance {
   std::string host;
   std::string port;
   std::string name;
};

inline
auto operator==(instance const& a, instance const& b)
{
  return a.host == b.host
      && a.port == b.port
      && a.name == b.name;
}

// Still on development.
template <class AsyncReadStream>
class sentinel_op2 {
public:
  struct config {
     // A list of redis sentinels e.g. ip1 port1 ip2 port2 ...
     std::vector<std::string> sentinels {"127.0.0.1", "26379"};
     std::string name {"mymaster"};
     std::string role {"master"};
  };

private:
   enum class op_state
   { on_connect
   , on_write
   , on_read
   };

   struct impl {
      AsyncReadStream& stream;
      resp::buffer buffer;
      resp::response res;
      instance* inst;
      op_state opstate {op_state::on_connect};
      std::string cmd;
      config cfg;
      ip::tcp::resolver resv;

      impl(AsyncReadStream& s, config c, instance* i)
      : stream(s)
      , resv(s.get_executor())
      , inst(i)
      , cfg(c)
      {}
   };

   std::shared_ptr<impl> impl_;

public:
   sentinel_op2(AsyncReadStream& stream, config cfg, instance* inst)
   : impl_(std::make_shared<impl>(stream, cfg, inst))
   {
      auto const n = std::size(cfg.sentinels);
      if (n == 0 || (n % 2 != 0))
	 throw std::runtime_error("sentinel_op2: wrong size.");
   }

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , std::size_t n = 0)
   {
      switch (impl_->opstate) {
      case op_state::on_connect:
      {
	 auto const n = std::size(impl_->cfg.sentinels) / 2;

	 unsigned i = 0;
	 for (i = 0; i < n; ++i) {
	    auto const res = impl_->resv
	       .resolve( impl_->cfg.sentinels[2 * i + 0]
		       , impl_->cfg.sentinels[2 * i + 1]
		       , ec);
	    if (ec)
	       return self.complete(ec);

	    net::connect(impl_->stream, res, ec);
	    if (!ec)
	       break;

	    if (ec && ((2 * (i + 1)) == std::size(impl_->cfg.sentinels)))
	       return self.complete(ec);
	 }

	 // The redis documentation recommends to put the first
	 // sentinel that replies in the start of the list. See
	 // https://redis.io/topics/sentinel-clients
	 //
	 // TODO: The sentinel that responded has to be returned to
	 // the user so he can do what the doc describes above.
	 // Example
	 //
	 //   std::swap(cfg.sentinels[0], cfg.sentinels[2 * i + 0]);
	 //   std::swap(cfg.sentinels[1], cfg.sentinels[2 * i + 1]);

         impl_->inst->name = impl_->cfg.name;
         impl_->cmd = sentinel("get-master-addr-by-name", impl_->inst->name);
         impl_->opstate = op_state::on_write;
         net::async_write( impl_->stream
                         , net::buffer(impl_->cmd)
                         , std::move(self));
      } break;
      case op_state::on_write:
      {
         if (ec)
            return self.complete(ec);

         impl_->opstate = op_state::on_read;

         resp::async_read(
	    impl_->stream,
	    impl_->buffer,
	    impl_->res,
	    std::move(self));

      } break;
      case op_state::on_read:
      {
         auto n = std::size(impl_->res.res);
         if (n > 1) {
            impl_->inst->host = impl_->res.res[0];
            impl_->inst->port = impl_->res.res[1];
         }
         self.complete(ec);
      } break;
      default: { }
      }
   }
};

template <
   class AsyncReadStream,
   class CompletionToken =
     net::default_completion_token_t<typename AsyncReadStream::executor_type>
   >
auto async_get_instance2(
  AsyncReadStream& stream,
  typename sentinel_op2<AsyncReadStream>::config cfg,
  instance& inst,
  CompletionToken&& token =
     net::default_completion_token_t<typename AsyncReadStream::executor_type>{})
{
   return net::async_compose
      < CompletionToken
      , void(boost::system::error_code)
      >(sentinel_op2<AsyncReadStream> {stream, cfg, &inst},
        token,
        stream);
}

template <class AsyncReadStream>
class session {
public:
   using on_conn_handler_type = std::function<void()>;

   using msg_handler_type =
      std::function<void( boost::system::error_code const&
                        , std::vector<std::string>)>;

   using sentinel_config = typename sentinel_op2<AsyncReadStream>::config;

   struct config {
     using sentinel_config = typename sentinel_op2<AsyncReadStream>::config;
     sentinel_config sentinel;
     int max_pipeline_size {256};
     log::level log_filter {log::level::debug};
   };

private:
   struct queue_item {
      std::string payload;
      bool sent;
   };

   std::string id_;
   config cfg_;
   ip::tcp::resolver resolver_;
   AsyncReadStream stream_;

   resp::buffer buffer_;
   resp::response res_;
   std::queue<queue_item> msg_queue_;
   int pipeline_size_ = 0;
   long long pipeline_id_ = 0;
   instance instance_;
   bool disable_reconnect_ = false;

   msg_handler_type msg_handler_ = [this](auto ec, auto const& res)
   {
      if (ec) {
         log::write( cfg_.log_filter
                   , log::level::debug
                   , "{0}/msg_handler: {1}."
                   , id_
                   , ec.message());
      }

      std::copy( std::cbegin(res)
               , std::cend(res)
               , std::ostream_iterator<std::string>(std::cout, " "));

      std::cout << std::endl;
   };


   on_conn_handler_type conn_handler_ = [](){};

   void do_read_resp()
   {
      res_.clear();

      auto f = [this](auto const& ec)
         { on_resp(ec); };

      resp::async_read(
	 stream_,
	 buffer_,
	 res_,
	 std::move(f));
   }

   void
   on_instance( std::string const& host
              , std::string const& port
              , boost::system::error_code ec)
   {
      buffer_.clear();
      res_.clear();

      if (ec) {
         log::write( cfg_.log_filter
                   , log::level::warning
                   , "{0}/on_instance: {1}. Endpoint: {2}"
                   , id_
                   , ec.message());
         return;
      }

      // Closes the connection with the sentinel and connects with the
      // master.
      close("{0}/on_instance: {1}.");

      // NOTE: Call sync resolve to prevent asio from starting a new
      // thread.
      ec = {};
      auto res = resolver_.resolve(host, port, ec);
      if (ec) {
         log::write( cfg_.log_filter
                   , log::level::warning
                   , "{0}/on_instance: {1}."
                   , id_
                   , ec.message());
         return;
      }

      do_connect(res);
   }

   void do_connect(net::ip::tcp::resolver::results_type res)
   {
      auto f = [this](auto ec, auto iter)
         { on_connect(ec, iter); };

      net::async_connect(stream_, res, f);
   }

   void on_connect( boost::system::error_code ec
                  , net::ip::tcp::endpoint const& endpoint)
   {
      if (ec) {
         log::write( cfg_.log_filter
                   , log::level::warning
                   , "{0}/on_connect: {1}. Endpoint: {2}"
                   , id_
                   , ec.message()
                   , endpoint);

         if (!disable_reconnect_)
            run();

         return;
      }

      log::write( cfg_.log_filter
                , log::level::info
                , "{0}/Success connecting to redis instance {1}"
                , id_
                , endpoint);

      do_read_resp();

      // Consumes any messages that have been eventually posted while the
      // connection was not established, or consumes msgs when a
      // connection to redis is restablished.
      if (!std::empty(msg_queue_)) {
         log::write( cfg_.log_filter
                   , log::level::debug
                   , "{0}/on_connect: Number of messages {1}"
                   , id_
                   , std::size(msg_queue_));
         do_write();
      }

      // Calls user callback to inform a successfull connect to redis.
      // It may wish to start sending some commands.
      //
      // Since this callback may call the send member function on this
      // object, we have to call it AFTER the write operation above,
      // otherwise the message will be sent twice.
      conn_handler_();
   }

   void on_resp(boost::system::error_code ec)
   {
      if (ec) {
         log::write( cfg_.log_filter
                   , log::level::warning
                   , "{0}/on_resp: {1}."
                   , id_
                   , ec.message());

         // Some of the possible errors are.
         // net::error::eof
         // net::error::connection_reset
         // net::error::operation_aborted

         close("{0}/on_resp: {1}.");
         if (!disable_reconnect_)
            run();

         return;
      }

      msg_handler_(ec, std::move(res_.res));

      do_read_resp();

      if (std::empty(msg_queue_))
         return;

      // In practive, the if condition below will always hold as we pop the
      // last written message as soon as the first response from a pipeline
      // is received and send the next. If think the code is clearer this
      // way.
      if (msg_queue_.front().sent) {
         msg_queue_.pop();

         if (std::empty(msg_queue_))
            return;

         do_write();
      }
   }

   void on_write(boost::system::error_code ec, std::size_t n)
   {
      if (ec) {
         log::write( cfg_.log_filter
                   , log::level::info
                   , "{0}/on_write: {1}."
                   , id_, ec.message());
         return;
      }
   }

   void do_write()
   {
      auto f = [this](auto ec, auto n)
         { on_write(ec, n); };

      assert(!std::empty(msg_queue_));
      assert(!std::empty(msg_queue_.front().payload));

      net::async_write( stream_
                      , net::buffer(msg_queue_.front().payload)
                      , f);
      msg_queue_.front().sent = true;
   }

   void close(char const* msg)
   {
      boost::system::error_code ec;
      stream_.close(ec);
      if (ec) {
         log::write( cfg_.log_filter
                   , log::level::warning
                   , msg
                   , id_
                   , ec.message());
      }
   }

public:
   session( net::io_context& ioc
          , config cfg
          , std::string id = "aedis")
   : id_(id)
   , cfg_ {std::move(cfg)}
   , resolver_ {ioc} 
   , stream_ {ioc}
   {
      if (cfg_.max_pipeline_size < 1)
         cfg_.max_pipeline_size = 1;
   }

   session(net::io_context& ioc) : session {ioc, {}, {}} { }

   void set_on_conn_handler(on_conn_handler_type f)
      { conn_handler_ = std::move(f);};

   void set_msg_handler(msg_handler_type f)
      { msg_handler_ = std::move(f);};

   auto send(std::string msg)
   {
      assert(!std::empty(msg));

      auto const max_pp_size_reached =
         pipeline_size_ >= cfg_.max_pipeline_size;

      if (max_pp_size_reached)
         pipeline_size_ = 0;

      auto const is_empty = std::empty(msg_queue_);

      // When std::size(msg_queue_) == 1 we know the message in the back of
      // queue has already been sent and we are waiting for a reponse, we
      // cannot pipeline in this case.
      if (is_empty || std::size(msg_queue_) == 1 || max_pp_size_reached) {
         msg_queue_.push({std::move(msg), false});
	 ++pipeline_id_;
      } else {
         msg_queue_.back().payload += msg; // Uses pipeline.
         ++pipeline_size_;
      }

      if (is_empty && stream_.is_open())
         do_write();

      return pipeline_id_;
   }

   void run()
   {
      auto f = [this](auto ec)
         { on_instance(instance_.host, instance_.port, ec); };

      async_get_instance2(stream_, cfg_.sentinel, instance_, f);
   }

   void disable_reconnect()
   {
      assert(!disable_reconnect_);
      disable_reconnect_ = true;
   }
};

}

