/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres at gmail dot com)
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

struct buffer {
   std::string data;
   std::vector<std::string> res;

   void clear()
   {
      data.clear();
      res.clear();
   }
};

inline
std::string make_bulky_item(std::string const& param)
{
   auto const s = std::size(param);
   return "$"
        + std::to_string(s)
        + "\r\n"
        + param
        + "\r\n";
}

inline
std::string make_header(int size)
{
   return "*" + std::to_string(size) + "\r\n";
}

struct accumulator {
   auto operator()(std::string a, std::string b) const
   {
      a += make_bulky_item(b);
      return a;
   }

   auto operator()(std::string a, int b) const
   {
      a += make_bulky_item(std::to_string(b));
      return a;
   }

   auto operator()(std::string a, std::pair<std::string, std::string> b) const
   {
      a += make_bulky_item(b.first);
      a += make_bulky_item(b.second);
      return a;
   }

   auto operator()(std::string a, std::pair<int, std::string> b) const
   {
      a += make_bulky_item(std::to_string(b.first));
      a += make_bulky_item(b.second);
      return a;
   }
};

inline
auto assemble(char const* cmd)
{
   return make_header(1) + make_bulky_item(cmd);
}

template <class Iter>
auto assemble( char const* cmd
             , std::initializer_list<std::string const> key
             , Iter begin
             , Iter end
             , int size = 1)
{
   auto const d1 =
      std::distance( std::cbegin(key)
                   , std::cend(key));

   auto const d2 = std::distance(begin, end);

   auto a = make_header(1 + d1 + size * d2)
          + make_bulky_item(cmd);

   auto b =
      std::accumulate( std::cbegin(key)
                     , std::cend(key)
                     , std::move(a)
                     , accumulator{});

   return
      std::accumulate( begin
                     , end
                     , std::move(b)
                     , accumulator{});
}

inline
auto assemble(char const* cmd, std::string const& key)
{
   std::initializer_list<std::string> dummy;
   return assemble(cmd, {key}, std::cbegin(dummy), std::cend(dummy));
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

struct read_op {
   tcp::socket& socket_;
   resp::buffer* buffer_ = nullptr;
   int start_ = 1;
   int counter_ = 1;
   bool bulky_read_ = false;

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code const& ec = {}
                  , std::size_t n = 0)
   {
      switch (start_) {
         for (;;) {
            case 1:
            start_ = 0;
            net::async_read_until( socket_
                                 , net::dynamic_buffer(buffer_->data)
                                 , "\r\n"
                                 , std::move(self));
            return; default:

            if (ec || n < 3) {
               self.complete(ec);
               return;
            }

            auto str_flag = false;
            if (bulky_read_) {
               buffer_->res.push_back(buffer_->data.substr(0, n - 2));
               --counter_;
            } else {
               if (counter_ != 0) {
                  switch (buffer_->data.front()) {
                     case '$':
                     {
                        // We may want to consider not pushing in the vector
                        // but find a way to report nil.
                        if (buffer_->data.compare(1, 2, "-1") == 0) {
                           buffer_->res.push_back({});
                           --counter_;
                        } else {
                           str_flag = true;
                        }
                     }
                     break;
                     case '+':
                     case '-':
                     case ':':
                     {
                        buffer_->res.push_back(buffer_->data.substr(1, n - 3));
                        --counter_;
                     }
                     break;
                     case '*':
                     {
                        //assert(counter_ == 1);
                        counter_ = get_length(buffer_->data.data() + 1);
                     }
                     break;
                     default:
                        assert(false);
                  }
               }
            }

            buffer_->data.erase(0, n);

            if (counter_ == 0) {
               self.complete(boost::system::error_code{});
               return;
            }

            bulky_read_ = str_flag;
         }
      }
   }
};

template <class CompletionToken>
auto async_read(tcp::socket& s, resp::buffer* buffer, CompletionToken&& token)
{
   return net::async_compose
      < CompletionToken
      , void(boost::system::error_code)
      >(read_op {s, buffer}, token, s);
}

}

inline
auto sentinel(std::string const& arg, std::string const& name)
{
   auto par = {name};
   return resp::assemble("SENTINEL", {arg}, std::cbegin(par), std::cend(par));
}

inline
auto append(std::string const& key, std::string const& msg)
{
   auto par = {msg};
   return resp::assemble("APPEND", {key}, std::cbegin(par), std::cend(par));
}

inline
auto auth(std::string const& pwd)
{
   return resp::assemble("AUTH", pwd);
}

inline
auto bgrewriteaof()
{
   return resp::assemble("BGREWRITEAOF");
}

inline
auto role()
{
   return resp::assemble("ROLE");
}

inline
auto bgsave()
{
   return resp::assemble("BGSAVE");
}

inline
auto bitcount(std::string const& key, int start = 0, int end = -1)
{
   auto par = {std::to_string(start), std::to_string(end)};

   return 
      resp::assemble( "BITCOUNT"
                    , {key}
                    , std::cbegin(par)
                    , std::cend(par));
}

template <class Iter>
auto rpush(std::string const& key, Iter begin, Iter end)
{
   return resp::assemble("RPUSH", {key}, begin, end);
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
   return resp::assemble("LPUSH", {key}, begin, end);
}

inline
auto multi()
{
   return resp::assemble("MULTI");
}

inline
auto ping()
{
   return resp::assemble("PING");
}

inline
auto flushall()
{
   return resp::assemble("FLUSHALL");
}

inline
auto exec()
{
   return resp::assemble("EXEC");
}

inline
auto incr(std::string const& key)
{
   return resp::assemble("INCR", key);
}

inline
auto lpop(std::string const& key)
{
   return resp::assemble("LPOP", key);
}

inline
auto subscribe(std::string const& key)
{
   return resp::assemble("SUBSCRIBE", key);
}

inline
auto unsubscribe(std::string const& key)
{
   return resp::assemble("UNSUBSCRIBE", key);
}

inline
auto get(std::string const& key)
{
   return resp::assemble("GET", key);
}

inline
auto publish(std::string const& key, std::string const& msg)
{
   auto par = {msg};
   return resp::assemble("PUBLISH", {key}, std::cbegin(par), std::cend(par));
}

inline
auto set( std::string const& key
        , std::initializer_list<std::string const> const& args)
{
   return resp::assemble("SET", {key}, std::cbegin(args), std::cend(args));
}

inline
auto hset( std::string const& key
         , std::initializer_list<std::string const> const& l)
{
   return resp::assemble("HSET", {key}, std::cbegin(l), std::cend(l));
}

template <class Key, class T, class Compare, class Allocator>
auto hset( std::string const& key
         , std::map<Key, T, Compare, Allocator> const& m)
{
   return resp::assemble("HSET", {key}, std::cbegin(m), std::cend(m), 2);
}

inline
auto hvals(std::string const& key)
{
   return resp::assemble("HVALS", {key});
}

inline
auto hget(std::string const& key, std::string const& field)
{
   auto par = {field};
   return resp::assemble("HGET", {key}, std::cbegin(par), std::cend(par));
}

inline
auto hmget( std::string const& key
          , std::string const& field1
          , std::string const& field2)
{
   auto par = {field1, field2};
   return resp::assemble("HMGET", {key}, std::cbegin(par), std::cend(par));
}

inline
auto expire(std::string const& key, int secs)
{
   auto par = {std::to_string(secs)};
   return resp::assemble("EXPIRE", {key}, std::cbegin(par), std::cend(par));
}

inline
auto zadd(std::string const& key, int score, std::string const& value)
{
   auto par = {std::to_string(score), value};
   return resp::assemble("ZADD", {key}, std::cbegin(par), std::cend(par));
}

template <class Key, class T, class Compare, class Allocator>
auto zadd( std::initializer_list<std::string const> key
         , std::map<Key, T, Compare, Allocator> const& m)
{
   return resp::assemble("ZADD", key, std::cbegin(m), std::cend(m), 2);
}

inline
auto zrange(std::string const& key, int min = 0, int max = -1)
{
   auto par = { std::to_string(min), std::to_string(max) };
   return resp::assemble("ZRANGE", {key}, std::cbegin(par), std::cend(par));
}

inline
auto zrangebyscore(std::string const& key, int min, int max)
{
   auto max_str = std::string {"inf"};
   if (max != -1)
      max_str = std::to_string(max);

   auto par = { std::to_string(min) , max_str };
   return resp::assemble("zrangebyscore", {key}, std::cbegin(par), std::cend(par));
}

inline
auto zremrangebyscore(std::string const& key, int score)
{
   auto const s = std::to_string(score);
   auto par = {s, s};
   return resp::assemble("ZREMRANGEBYSCORE", {key}, std::cbegin(par), std::cend(par));
}

inline
auto lrange(std::string const& key, int min = 0, int max = -1)
{
   auto par = { std::to_string(min) , std::to_string(max) };
   return resp::assemble("lrange", {key}, std::cbegin(par), std::cend(par));
}

inline
auto del(std::string const& key)
{
   return resp::assemble("del", key);
}

inline
auto llen(std::string const& key)
{
   return resp::assemble("llen", key);
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

struct instance_op {
   enum class states
   { starting
   , writing
   , waiting };

   tcp::socket& socket_;
   resp::buffer* buffer_;
   instance* instance_;
   states state_ {states::starting};
   std::string cmd_;

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , std::size_t n = 0)
   {
      switch (state_) {
      case states::starting:
      {
         cmd_ = sentinel("get-master-addr-by-name", instance_->name);
         state_ = states::writing;
         net::async_write( socket_
                         , net::buffer(cmd_)
                         , std::move(self));
      } break;
      case states::writing:
      {
         if (ec)
            return self.complete(ec);

         state_ = states::waiting;
         resp::async_read(socket_, buffer_, std::move(self));

      } break;
      case states::waiting:
      {
         auto n = std::size(buffer_->res);
         if (n > 1) {
            instance_->host = buffer_->res[0];
            instance_->port = buffer_->res[1];
         }
         self.complete(ec);
      } break;
      default:
      {
      }
      }
   }
};

template <class CompletionToken>
auto
async_get_instance( tcp::socket& s
                  , resp::buffer* p
                  , instance* p2
                  , CompletionToken&& token)
{
   return net::async_compose
      < CompletionToken
      , void(boost::system::error_code)
      >(instance_op {s, p, p2}, token, s);
}

class session {
public:
   using on_conn_handler_type = std::function<void()>;

   using msg_handler_type =
      std::function<void( boost::system::error_code const&
                        , std::vector<std::string>)>;

  struct config {
     // A list of redis sentinels e.g. ip1 port1 ip2 port2 ...
     std::vector<std::string> sentinels {"127.0.0.1", "26379"};
     std::string name {"mymaster"};
     std::string role {"master"};
     int max_pipeline_size {256};
     log::level log_filter {log::level::debug};
  };

private:
   std::string id_;
   config cfg_;
   ip::tcp::resolver resolver_;
   tcp::socket socket_;

   net::steady_timer timer_;
   resp::buffer buffer_;
   std::queue<std::string> msg_queue_;
   int pipeline_size_ = 0;
   long long pipeline_id_ = 0;
   instance instance_;
   int sentinel_idx_ = 0;

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
      buffer_.res.clear();

      auto f = [this](auto const& ec)
         { on_resp(ec); };

      resp::async_read(socket_, &buffer_, std::move(f));
   }

   void
   on_instance( std::string const& host
              , std::string const& port
              , boost::system::error_code ec)
   {
      buffer_.clear();

      if (ec) {
         log::write( cfg_.log_filter
                   , log::level::warning
                   , "{0}/on_instance: {1}. Endpoint: {2}"
                   , id_
                   , ec.message());
         return;
      }

      // Closes the connection with the sentinel and connects with the master.
      ec = {};
      socket_.close(ec);
      if (ec) {
         log::write( cfg_.log_filter
                   , log::level::warning
                   , "{0}/on_instance: {1}."
                   , id_
                   , ec.message());
         return;
      }

      // NOTE: Call sync resolve to prevent asio from starting a new thread.
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

      net::async_connect(socket_, res, f);
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
         close();
         run();
         return;
      }

      msg_handler_(ec, std::move(buffer_.res));

      do_read_resp();

      if (std::empty(msg_queue_))
         return;

      if (!std::empty(msg_queue_))
         do_write();
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

      msg_queue_.pop();
   }

   void do_write()
   {
      auto f = [this](auto ec, auto n)
         { on_write(ec, n); };

      net::async_write( socket_
                      , net::buffer(msg_queue_.front())
                      , f);
   }

public:
   session( net::io_context& ioc
          , config cfg
          , std::string id = {})
   : id_(id)
   , cfg_ {std::move(cfg)}
   , resolver_ {ioc} 
   , socket_ {ioc}
   , timer_ {ioc, std::chrono::steady_clock::time_point::max()}
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
      auto const max_pp_size_reached =
         pipeline_size_ >= cfg_.max_pipeline_size;

      if (max_pp_size_reached)
         pipeline_size_ = 0;

      auto const is_empty = std::empty(msg_queue_);

      if (is_empty || std::size(msg_queue_) == 1 || max_pp_size_reached) {
         msg_queue_.push(std::move(msg));
	 ++pipeline_id_;
      } else {
         msg_queue_.back() += msg; // Uses pipeline.
         ++pipeline_size_;
      }

      if (is_empty && socket_.is_open())
         do_write();

      return pipeline_id_;
   }

   void close()
   {
      boost::system::error_code ec;
      socket_.shutdown(net::ip::tcp::socket::shutdown_send, ec);

      if (ec) {
         log::write( cfg_.log_filter
                   , log::level::warning
                   , "{0}/close: {1}."
                   , id_
                   , ec.message());
      }

      ec = {};
      socket_.close(ec);
      if (ec) {
         log::write( cfg_.log_filter
                   , log::level::warning
                   , "{0}/close: {1}."
                   , id_
                   , ec.message());
      }
   }

   void
   on_sentinel_conn( boost::system::error_code ec
                   , net::ip::tcp::endpoint const& endpoint)
   {
      if (ec) {
         log::write( cfg_.log_filter
                   , log::level::warning
                   , "{0}/on_sentinel_conn: {1}. Endpoint: {2}"
                   , id_
                   , ec.message()
                   , endpoint);

         // Ask the next sentinel only if we did not try them all yet.
         ++sentinel_idx_;
         if ((2 * sentinel_idx_) == std::size(cfg_.sentinels)) {
            log::write( cfg_.log_filter
                      , log::level::warning
                      , "{0}/No sentinel knows the redis instance address."
                      , id_);

            return;
         }

         run(); // Tries the next sentinel.
         return;
      }

      // The redis documentation recommends to put the first sentinel that
      // replies in the start of the list. See
      // https://redis.io/topics/sentinel-clients
      if (sentinel_idx_ != 0) {
         auto const r = sentinel_idx_;
         std::swap(cfg_.sentinels[0], cfg_.sentinels[2 * r]);
         std::swap(cfg_.sentinels[1], cfg_.sentinels[2 * r + 1]);
         sentinel_idx_ = 0;
      }

      log::write( cfg_.log_filter
                , log::level::info
                , "{0}/Success connecting to sentinel {1}"
                , id_
                , endpoint);

      auto g = [this](auto ec)
         { on_instance(instance_.host, instance_.port, ec); };

      instance_.name = cfg_.name;
      async_get_instance(socket_, &buffer_, &instance_, std::move(g));
   }

   void run()
   {
      auto const n = std::size(cfg_.sentinels);

      if (n == 0 || (n % 2 != 0)) {
         log::write( cfg_.log_filter
                   , log::level::warning
                   , "{0}/run: Incompatible sentinels array size: {1}"
                   , id_, n);
         return;
      }

      auto const r = sentinel_idx_;

      boost::system::error_code ec;
      auto res = resolver_
         .resolve( cfg_.sentinels[2 * r]
                 , cfg_.sentinels[2 * r + 1]
                 , ec);

      if (ec) {
         log::write( cfg_.log_filter
                   , log::level::warning
                   , "{0}/run: Can't resolve sentinel: {1}."
                   , id_
                   , ec.message());
         return;
      }

      auto f = [this](auto ec, auto iter)
      { on_sentinel_conn(ec, iter); };

      net::async_connect(socket_, res, f);
   }
};

}

