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

template <class AsyncStream, class Handler>
struct read_resp_op {
   AsyncStream& stream_;
   Handler handler_;
   resp::buffer* buffer_ = nullptr;
   int start_ = 0;
   int counter_ = 1;
   bool bulky_read_ = false;

   read_resp_op( AsyncStream& stream
               , resp::buffer* buffer
               , Handler handler)
   : stream_(stream)
   , handler_(std::move(handler))
   , buffer_(buffer)
   { }

   void operator()( boost::system::error_code const& ec, std::size_t n
                  , int start = 0)
   {
      switch (start_ = start) {
         for (;;) {
            case 1:
            net::async_read_until( stream_, net::dynamic_buffer(buffer_->data)
                                 , "\r\n", std::move(*this));
            return; default:

            if (ec || n < 3) {
               handler_(ec);
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
                        // TODO: Do not push in the vector but find a way to
                        // report nil.
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
               handler_(boost::system::error_code{});
               return;
            }

            bulky_read_ = str_flag;
         }
      }
   }
};

template < class AsyncReadStream
         , class ReadHandler
         >
inline bool
asio_handler_is_continuation( read_resp_op< AsyncReadStream
                                          , ReadHandler
                                          >* this_handler)
{
   return this_handler->start_ == 0 ? true
      : boost_asio_handler_cont_helpers::is_continuation(
            this_handler->handler_);
}

template <class AsyncStream, class CompletionToken>
auto async_read_resp( AsyncStream& s
                    , resp::buffer* buffer
                    , CompletionToken&& handler)
{
   using read_handler_signature = void (boost::system::error_code const&);

   net::async_completion< CompletionToken
                        , read_handler_signature
                        > init {handler};

   using handler_type = 
      read_resp_op< AsyncStream
                  , BOOST_ASIO_HANDLER_TYPE( CompletionToken
                                           , read_handler_signature)
                  >;

   handler_type {s, buffer, init.completion_handler}({}, 0, 1);

   return init.result.get();
}

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
          , std::list<T, Allocator> const& l)
{
   return rpush(key, std::cbegin(l), std::cend(l));
}

template <class T, class Compare, class Allocator>
auto rpush( std::string const& key
          , std::set<T, Compare, Allocator> const& s)
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
   return resp::assemble("PUBLISH", {key}, std::begin(par), std::end(par));
}

inline
auto set( std::string const& key
        , std::initializer_list<std::string const> const& args)
{
   return resp::assemble("SET", {key}, std::begin(args), std::end(args));
}

inline
auto hset( std::string const& key
         , std::initializer_list<std::string const> const& l)
{
   return resp::assemble("HSET", {key}, std::begin(l), std::end(l));
}

template <class Key, class T, class Compare, class Allocator>
auto hset( std::string const& key
         , std::map<Key, T, Compare, Allocator> const& m)
{
   return resp::assemble("HSET", {key}, std::begin(m), std::end(m), 2);
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
   return resp::assemble("HGET", {key}, std::begin(par), std::end(par));
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
   return resp::assemble("EXPIRE", {key}, std::begin(par), std::end(par));
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
   return resp::assemble("ZRANGE", {key}, std::begin(par), std::end(par));
}

inline
auto zrangebyscore(std::string const& key, int min, int max)
{
   auto max_str = std::string {"inf"};
   if (max != -1)
      max_str = std::to_string(max);

   auto par = { std::to_string(min) , max_str };
   return resp::assemble("zrangebyscore", {key}, std::begin(par), std::end(par));
}

inline
auto zremrangebyscore(std::string const& key, int score)
{
   auto const s = std::to_string(score);
   auto par = {s, s};
   return resp::assemble("ZREMRANGEBYSCORE", {key}, std::begin(par), std::end(par));
}

inline
auto lrange(std::string const& key, int min = 0, int max = -1)
{
   auto par = { std::to_string(min) , std::to_string(max) };
   return resp::assemble("lrange", {key}, std::begin(par), std::end(par));
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

class session {
public:
   using on_conn_handler_type = std::function<void()>;

   using on_disconnect_handler_type =
      std::function<void(boost::system::error_code const&)>;

   using msg_handler_type =
      std::function<void( boost::system::error_code const&
                        , std::vector<std::string>)>;

  struct config {
     std::string host {"127.0.0.1"};
     std::string port {"6379"};
     int max_pipeline_size {256};

     // If the connection to redis is lost, the session tries to reconnect with
     // this interval. This will be removed after the implementation of redis
     // sentinel is finished.
     std::chrono::milliseconds conn_retry_interval {500};

     // A list of redis sentinels e.g. ip1:port1 ip2:port2 ...
     // Not yet in use.
     std::vector<std::string> sentinels;

     log::level log_filter {log::level::debug};
  };

private:
   std::string id_;
   config cfg_;
   net::ip::tcp::resolver resolver_;
   net::ip::tcp::socket socket_;

   // This variable will be removed after the redis sentinel implementation. 
   net::steady_timer timer_;
   resp::buffer buffer_;
   std::queue<std::string> msg_queue_;
   int pipeline_counter_ = 0;

   msg_handler_type msg_handler_ = [this](auto ec, auto const& res)
   {
      if (ec) {
         log::write( cfg_.log_filter
                   , log::level::debug
                   , "{0}/msg_handler_: {1}."
                   , id_
                   , ec.message());
      }

      std::copy( std::cbegin(res)
               , std::cend(res)
               , std::ostream_iterator<std::string>(std::cout, " "));

      std::cout << std::endl;
   };


   on_conn_handler_type conn_handler_ = [](){};
   on_disconnect_handler_type disconn_handler_ = [](auto const&){};

   void start_reading_resp()
   {
      buffer_.res.clear();

      auto handler = [this](auto const& ec)
         { on_resp(ec); };

      resp::async_read_resp(socket_, &buffer_, handler);
   }

   void on_resolve( boost::system::error_code const& ec
                  , net::ip::tcp::resolver::results_type results)
   {
      if (ec) {
         log::write( cfg_.log_filter
                   , log::level::warning
                   , "{0}/on_resolve: {1}."
                   , id_, ec.message());
         return;
      }

      auto handler = [this](auto ec, auto iter)
         { on_connect(ec, iter); };

      net::async_connect(socket_, results, handler);
   }

   void on_connect( boost::system::error_code const& ec
                  , net::ip::tcp::endpoint const& endpoint)
   {
      if (ec) {
         return;
      }

      log::write( cfg_.log_filter
                , log::level::info
                , "{0}/on_connect: Success connecting to {1}"
                , id_
                , endpoint);

      start_reading_resp();

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

   void on_resp(boost::system::error_code const& ec)
   {
      if (ec) {
         log::write( cfg_.log_filter
                   , log::level::warning
                   , "{0}/on_resp1: {1}."
                   , id_
                   , ec.message());

         auto const b1 = ec == net::error::eof;
         auto const b2 = ec == net::error::connection_reset;
         if (b1 || b2) {
            // Redis has cleanly closed the connection, we try to
            // reconnect.
            timer_.expires_after(cfg_.conn_retry_interval);

            auto const handler = [this](auto const& ec)
               { on_conn_closed(ec); };

            timer_.async_wait(handler);
            return;
         }

         if (ec == net::error::operation_aborted) {
            // The operation has been canceled, this can happen in only
            // one way
            //
            // 1. There has been a request from the worker to close the
            //    connection and leave. In this case we should NOT try to
            //    reconnect. We have nothing to do.
            return;
         }

         log::write( cfg_.log_filter
                   , log::level::warning
                   , "{0}/on_resp2: Unhandled error '{1}'."
                   , id_, ec.message());

         return;
      }

      msg_handler_(ec, std::move(buffer_.res));

      start_reading_resp();

      if (std::empty(msg_queue_))
         return;

      msg_queue_.pop();

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
   }

   void do_write()
   {
      auto handler = [this](auto ec, auto n)
         { on_write(ec, n); };

      net::async_write(socket_, net::buffer(msg_queue_.front()), handler);
   }

   void on_conn_closed(boost::system::error_code ec)
   {
      if (ec) {
         if (ec == net::error::operation_aborted) {
            // The timer has been canceled. Probably somebody
            // shutting down the application while we are trying to
            // reconnect.
            return;
         }

         log::write( cfg_.log_filter
                   , log::level::warning
                   , "{0}/on_conn_closed: {1}"
                   , id_
                   , ec.message());

         return;
      }

      // Given that the peer has shutdown the connection (I think)
      // we do not need to call shutdown.
      //socket_.shutdown(net::ip::tcp::socket::shutdown_both, ec);
      socket_.close(ec);

      // Instead of simply trying to reconnect I will run the
      // resolver again. This will be changes when sentinel
      // support is implemented.
      run();
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

   void set_on_conn_handler(on_conn_handler_type handler)
      { conn_handler_ = std::move(handler);};

   void set_on_disconnect_handler(on_disconnect_handler_type handler)
      { disconn_handler_ = std::move(handler);};

   void set_msg_handler(msg_handler_type handler)
      { msg_handler_ = std::move(handler);};

   void send(std::string msg)
   {
      auto const max_pp_size_reached =
         pipeline_counter_ >= cfg_.max_pipeline_size;

      if (max_pp_size_reached) {
         pipeline_counter_ = 0;
      }

      auto const is_empty = std::empty(msg_queue_);

      if (is_empty || std::size(msg_queue_) == 1 || max_pp_size_reached) {
         msg_queue_.push(std::move(msg));
      } else {
         msg_queue_.back() += msg; // Uses pipeline.
         ++pipeline_counter_;
      }

      if (is_empty && socket_.is_open())
         do_write();
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

      timer_.cancel();
   }


   void run()
   {
      //auto addr = split(cfg_.sentinels.front());
      //std::cout << addr.first << " -- " << addr.second << std::endl;

      // Calling sync resolve to avoid starting a new thread.
      boost::system::error_code ec;
      auto res = resolver_.resolve(cfg_.host, cfg_.port, ec);
      on_resolve(ec, res);
   }
};

}

