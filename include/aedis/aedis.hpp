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
#include <charconv>

#include <boost/asio.hpp>

template <class Iter>
void print(Iter begin, Iter end)
{
   for (; begin != end; ++begin)
     std::cout << *begin << " ";
   std::cout << std::endl;
}

template <class Range>
void print(Range const& v)
{
   using std::cbegin;
   using std::cend;
   print(cbegin(v), cend(v));
}
namespace aedis
{

namespace net = boost::asio;
namespace ip = net::ip;
using tcp = ip::tcp;

namespace resp
{

using buffer = std::string;

struct response_throw {
   virtual void select_array(int n) { throw std::runtime_error("select_array: Has not been overridden."); }
   virtual void select_push(int n) { throw std::runtime_error("select_push: Has not been overridden."); }
   virtual void select_set(int n) { throw std::runtime_error("select_set: Has not been overridden."); }
   virtual void select_map(int n) { throw std::runtime_error("select_map: Has not been overridden."); }
   virtual void select_attribute(int n) { throw std::runtime_error("select_attribute: Has not been overridden."); }

   virtual void on_simple_string(std::string_view s) { throw std::runtime_error("on_simple_string: Has not been overridden."); }
   virtual void on_simple_error(std::string_view s) { throw std::runtime_error("on_simple_error: Has not been overridden."); }
   virtual void on_number(std::string_view s) { throw std::runtime_error("on_number: Has not been overridden."); }
   virtual void on_double(std::string_view s) { throw std::runtime_error("on_double: Has not been overridden."); }
   virtual void on_bool(std::string_view s) { throw std::runtime_error("on_bool: Has not been overridden."); }
   virtual void on_big_number(std::string_view s) { throw std::runtime_error("on_big_number: Has not been overridden."); }
   virtual void on_null() { throw std::runtime_error("on_null: Has not been overridden."); }
   virtual void on_blob_error(std::string_view s = {}) { throw std::runtime_error("on_blob_error: Has not been overridden."); }
   virtual void on_verbatim_string(std::string_view s = {}) { throw std::runtime_error("on_verbatim_string: Has not been overridden."); }
   virtual void on_blob_string(std::string_view s = {}) { throw std::runtime_error("on_blob_string: Has not been overridden."); }
   virtual void on_streamed_string_part(std::string_view s = {}) { throw std::runtime_error("on_streamed_string_part: Has not been overridden."); }
};

template <class T>
std::enable_if<std::is_integral<T>::value, void>::type
from_string_view(std::string_view s, T& n)
{
   auto r = std::from_chars(s.data(), s.data() + s.size(), n);
   if (r.ec == std::errc::invalid_argument)
      throw std::runtime_error("from_chars: Unable to convert");
}

void from_string_view(std::string_view s, std::string& r)
   { r = s; }

template <class T, class Allocator = std::allocator<T>>
struct response_list : response_throw {
   std::list<T, Allocator> result;
   void select_array(int n) override { }
   void on_blob_string(std::string_view s) override
   {
      T r;
      from_string_view(s, r);
      result.push_back(std::move(r));
   }
};

template<
   class Key,
   class Compare = std::less<Key>,
   class Allocator = std::allocator<Key>>
struct response_set : response_throw {
   std::set<Key, Compare, Allocator> result;
   void select_array(int n) override { }
   void select_set(int n) override { }
   void on_blob_string(std::string_view s) override
   {
      Key r;
      from_string_view(s, r);
      result.insert(std::end(result), std::move(r));
   }
};

// General purpose response. Copies the string reponses in the result
// vector.
struct response_vector {
private:
   void add(std::string_view s = {})
      { result.emplace_back(s.data(), std::size(s)); }

public:
   std::vector<std::string> result;

   void clear() { result.clear(); }
   auto size() const noexcept { return std::size(result); }

   void select_array(int n) { }
   void select_push(int n) { }
   void select_set(int n) { }
   void select_map(int n) { }
   void select_attribute(int n) { }

   void on_simple_string(std::string_view s) { add(s); }
   void on_simple_error(std::string_view s) { add(s); }
   void on_number(std::string_view s) { add(s); }
   void on_double(std::string_view s) { add(s); }
   void on_bool(std::string_view s) { add(s); }
   void on_big_number(std::string_view s) { add(s); }
   void on_null() { add(); }
   void on_blob_error(std::string_view s = {}) { add(s); }
   void on_verbatim_string(std::string_view s = {}) { add(s); }
   void on_blob_string(std::string_view s = {}) { add(s); }
   void on_streamed_string_part(std::string_view s = {}) { add(s); }
};

using response = response_vector;

// Converts a decimal number in ascii format to an integer.
inline
std::size_t length(char const* p)
{
   std::size_t len = 0;
   while (*p != '\r') {
       len = (10 * len) + (*p - '0');
       p++;
   }
   return len;
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

template <class Response>
class parser {
public:
   enum class bulk
   { blob_error
   , verbatim_string 
   , blob_string
   , streamed_string_part
   , none
   };

private:
   Response* res_ = nullptr;
   int depth_ = 0;
   int sizes_[6] = {2, 1, 1, 1, 1, 1}; // Streaming will require a bigger integer.
   bulk bulk_ = bulk::none;
   int bulk_length_ = std::numeric_limits<int>::max();

   auto on_array_impl(char const* data, int m = 1)
   {
      auto const l = length(data + 1);
      if (l == 0) {
	 --sizes_[depth_];
	 return l;
      }

      auto const size = m * l;
      sizes_[++depth_] = size;
      return size;
   }

   void on_array(char const* data)
      { res_->select_array(on_array_impl(data, 1)); }

   void on_push(char const* data)
      { res_->select_push(on_array_impl(data, 1)); }

   void on_set(char const* data)
      { res_->select_set(on_array_impl(data, 1)); }

   void on_map(char const* data)
      { res_->select_map(on_array_impl(data, 2)); }

   void on_attribute(char const* data)
      { res_->select_attribute(on_array_impl(data, 2)); }

   void on_null()
      { res_->on_null(); --sizes_[depth_]; }

   auto handle_simple_string(char const* data, std::size_t n)
   {
      --sizes_[depth_];
      return std::string_view {data + 1, n - 3};
   }

   void on_simple_string(char const* data, std::size_t n)
      { res_->on_simple_string(handle_simple_string(data, n)); }

   void on_simple_error(char const* data, std::size_t n)
      { res_->on_simple_error(handle_simple_string(data, n)); }

   void on_number(char const* data, std::size_t n)
      { res_->on_number(handle_simple_string(data, n)); }

   void on_double(char const* data, std::size_t n)
      { res_->on_double(handle_simple_string(data, n)); }

   void on_boolean(char const* data, std::size_t n)
      { res_->on_bool(handle_simple_string(data, n)); }

   void on_big_number(char const* data, std::size_t n)
      { res_->on_big_number(handle_simple_string(data, n)); }

   void on_bulk(bulk b, std::string_view s = {})
   {
      switch (b) {
	 case bulk::blob_error: res_->on_blob_error(s); break;
	 case bulk::verbatim_string: res_->on_verbatim_string(s); break;
	 case bulk::blob_string: res_->on_blob_string(s); break;
	 case bulk::streamed_string_part:
         {
           if (std::empty(s)) {
              sizes_[depth_] = 1;
           } else {
              res_->on_streamed_string_part(s);
           }
         } break;
	 default: assert(false);
      }

      --sizes_[depth_];
   }

   auto on_blob_error_impl(char const* data, bulk b)
   {
      auto const l = length(data + 1);
      if (l == -1 || l == 0) {
	 on_bulk(b);
	 return bulk::none;
      }

      bulk_length_ = l;
      return b;
   }

   auto on_streamed_string_size(char const* data)
      { return on_blob_error_impl(data, bulk::streamed_string_part); }

   auto on_blob_error(char const* data)
      { return on_blob_error_impl(data, bulk::blob_error); }

   auto on_verbatim_string(char const* data)
      { return on_blob_error_impl(data, bulk::verbatim_string); }

   auto on_blob_string(char const* data)
   {
      if (*(data + 1) == '?') {
	 sizes_[++depth_] = std::numeric_limits<int>::max();
	 return bulk::none;
      }

      return on_blob_error_impl(data, bulk::blob_string);
   }

public:
   parser(Response* res)
   : res_ {res}
   {}

   std::size_t advance(char const* data, std::size_t n)
   {
      auto next = bulk::none;
      if (bulk_ != bulk::none) {
         n = bulk_length_ + 2;
         on_bulk(bulk_, {data, (std::size_t)bulk_length_});
      } else {
         if (sizes_[depth_] != 0) {
            switch (*data) {
               case '!': next = on_blob_error(data); break;
               case '=': next = on_verbatim_string(data); break; 
               case '$': next = on_blob_string(data); break;
               case ';': next = on_streamed_string_size(data); break;
               case '-': on_simple_error(data, n); break;
               case ':': on_number(data, n); break;
               case ',': on_double(data, n); break;
               case '#': on_boolean(data, n); break;
               case '(': on_big_number(data, n); break;
               case '+': on_simple_string(data, n); break;
               case '_': on_null(); break;
               case '>': on_push(data); break;
               case '~': on_set(data); break;
               case '*': on_array(data); break;
               case '|': on_attribute(data); break;
               case '%': on_map(data); break;
               default: assert(false);
            }
         }
      }
      
      while (sizes_[depth_] == 0)
         --sizes_[--depth_];
      
      bulk_ = next;
      return n;
   }

   auto done() const noexcept
     { return depth_ == 0 && bulk_ == bulk::none; }

   auto bulk() const noexcept
     { return bulk_; }

   auto bulk_length() const noexcept
     { return bulk_length_; }
};

// The parser supports up to 5 levels of nested structures. The first
// element in the sizes stack is a sentinel and must be different from
// 1.
template <class AsyncReadStream, class Response>
class parse_op {
private:
   AsyncReadStream& stream_;
   resp::buffer* buf_ = nullptr;
   parser<Response> parser_;
   int start_ = 1;

public:
   parse_op(AsyncReadStream& stream, resp::buffer* buf, Response* res)
   : stream_ {stream}
   , buf_ {buf}
   , parser_ {res}
   { }

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , std::size_t n = 0)
   {
      switch (start_) {
         for (;;) {
            if (parser_.bulk() == parser<Response>::bulk::none) {
               case 1:
               start_ = 0;
               net::async_read_until(
                  stream_,
                  net::dynamic_buffer(*buf_),
                  "\r\n",
                  std::move(self));

               return;
            }

	    // On a bulk read we can't read until delimiter since the
	    // payload may contain the delimiter itself so we have to
	    // read the whole chunk. However if the bulk blob is small
	    // enough it may be already on the buffer buf_ we read
	    // last time. If it is, there is no need of initiating
	    // another async op otherwise we have to read the
	    // missing bytes.
            if (std::ssize(*buf_) < (parser_.bulk_length() + 2)) {
               start_ = 0;
	       auto const s = std::ssize(*buf_);
	       auto const l = parser_.bulk_length();
               buf_->resize(l + 2);
               net::async_read(
                  stream_,
                  net::buffer(buf_->data() + s, l + 2 - s),
                  net::transfer_all(),
                  std::move(self));
               return;
            }

            default:
	    {
	       // The condition below is wrong. it must be n < 3 for case 1 
	       // and n < 2 for the async_read.
	       if (ec || n < 3)
		  return self.complete(ec);

	       n = parser_.advance(buf_->data(), n);

	       buf_->erase(0, n);
	       if (parser_.done())
		  return self.complete({});
	    }
         }
      }
   }
};

template <class SyncReadStream, class Response>
auto read(
   SyncReadStream& stream,
   resp::buffer& buf,
   Response& res,
   boost::system::error_code& ec)
{
   parser<Response> p {&res};
   std::size_t n = 0;
   do {
      if (p.bulk() == parser<Response>::bulk::none) {
	 n = net::read_until(stream, net::dynamic_buffer(buf), "\r\n", ec);
	 if (ec || n < 3)
	    return n;
      } else {
	 auto const s = std::ssize(buf);
	 auto const l = p.bulk_length();
	 if (s < (l + 2)) {
	    buf.resize(l + 2);
	    n = net::read(stream, net::buffer(buf.data() + s, l + 2 - s));
	    if (ec || n < 2)
	       return n;
	 }
      }

      n = p.advance(buf.data(), n);

      buf.erase(0, n);
   } while (!p.done());

   return n;
}

template<class SyncReadStream, class Response>
std::size_t
read(
   SyncReadStream& stream,
   resp::buffer& buf,
   Response& res)
{
   boost::system::error_code ec;
   auto const n = read(stream, buf, res, ec);

   if (ec)
       BOOST_THROW_EXCEPTION(boost::system::system_error{ec});

   return n;
}

template <
   class AsyncReadStream,
   class Response,
   class CompletionToken =
      net::default_completion_token_t<typename AsyncReadStream::executor_type>
   >
auto async_read(
   AsyncReadStream& stream,
   resp::buffer& buffer,
   Response& res,
   CompletionToken&& token =
      net::default_completion_token_t<typename AsyncReadStream::executor_type>{})
{
   return net::async_compose
      < CompletionToken
      , void(boost::system::error_code)
      >(parse_op<AsyncReadStream, Response> {stream, &buffer, &res},
        token,
        stream);
}

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
   auto auth(std::string const& pwd)
     { resp::assemble(payload, "AUTH", pwd); }
   auto bgrewriteaof()
     { resp::assemble(payload, "BGREWRITEAOF"); }
   auto role()
     { resp::assemble(payload, "ROLE"); }
   auto bgsave()
     { resp::assemble(payload, "BGSAVE"); }
   auto flushall()
     { resp::assemble(payload, "FLUSHALL"); }
   auto lpop(std::string const& key)
     { resp::assemble(payload, "LPOP", key); }
   auto subscribe(std::string const& key)
     { resp::assemble(payload, "SUBSCRIBE", key); }
   auto unsubscribe(std::string const& key)
     { resp::assemble(payload, "UNSUBSCRIBE", key); }
   auto get(std::string const& key)
     { resp::assemble(payload, "GET", key); }
   void hello(std::string const& key)
      { resp::assemble(payload, "HELLO", key); }
   
   auto sentinel(std::string const& arg, std::string const& name)
   {
      auto par = {name};
      resp::assemble(payload, "SENTINEL", {arg}, std::cbegin(par), std::cend(par));
   }
   
   auto append(std::string const& key, std::string const& msg)
   {
      auto par = {msg};
      resp::assemble(payload, "APPEND", {key}, std::cbegin(par), std::cend(par));
   }
   
   auto bitcount(std::string const& key, int start = 0, int end = -1)
   {
      auto par = {std::to_string(start), std::to_string(end)};
      resp::assemble( payload
   	            , "BITCOUNT"
   		    , {key}
   		    , std::cbegin(par)
   		    , std::cend(par));
   }
   
   template <class Iter>
   auto rpush(std::string const& key, Iter begin, Iter end)
     { resp::assemble(payload, "RPUSH", {key}, begin, end); }
   
   template <class T>
   auto rpush( std::string const& key
             , std::initializer_list<T> v)
     { return rpush(key, std::cbegin(v), std::cend(v)); } 

   template <class Range>
   void rpush(std::string const& key, Range const& v)
   {
      using std::cbegin;
      using std::cend;
      rpush(key, cbegin(v), cend(v));
   }
   
   template <class Iter>
   auto lpush(std::string const& key, Iter begin, Iter end)
     { resp::assemble(payload, "LPUSH", {key}, begin, end); }
   
   auto psubscribe(std::initializer_list<std::string> l)
   {
      std::initializer_list<std::string> dummy;
      resp::assemble(payload, "PSUBSCRIBE", l, std::cbegin(dummy), std::cend(dummy));
   }
   
   auto publish(std::string const& key, std::string const& msg)
   {
      auto par = {msg};
      resp::assemble(payload, "PUBLISH", {key}, std::cbegin(par), std::cend(par));
   }
   
   auto set( std::string const& key , std::initializer_list<std::string> args)
     { resp::assemble(payload, "SET", {key}, std::cbegin(args), std::cend(args)); }

   auto hset( std::string const& key, std::initializer_list<std::string> l)
     { resp::assemble(payload, "HSET", {key}, std::cbegin(l), std::cend(l)); }

   template <class Key, class T, class Compare, class Allocator>
   auto hset( std::string const& key, std::map<Key, T, Compare, Allocator> const& m)
     { resp::assemble(payload, "HSET", {key}, std::cbegin(m), std::cend(m), 2); }
   
   auto hincrby(std::string const& key, std::string const& field, int by)
   {
      auto par = {field, std::to_string(by)};
      resp::assemble(payload, "HINCRBY", {key}, std::cbegin(par), std::cend(par));
   }
   
   auto hkeys(std::string const& key)
   {
      auto par = {""};
      resp::assemble(payload, "HKEYS", {key}, std::cbegin(par), std::cend(par));
   }
   
   auto hlen(std::string const& key)
     { resp::assemble(payload, "HLEN", {key}); }
   auto hgetall(std::string const& key)
     { resp::assemble(payload, "HGETALL", {key}); }
   auto hvals(std::string const& key)
     { resp::assemble(payload, "HVALS", {key}); }
   
   auto hget(std::string const& key, std::string const& field)
   {
      auto par = {field};
      resp::assemble(payload, "HGET", {key}, std::cbegin(par), std::cend(par));
   }
   
   auto hmget( std::string const& key
             , std::initializer_list<std::string> fields)
   {
      resp::assemble( payload
   	         , "HMGET"
   		 , {key}
   		 , std::cbegin(fields)
   		 , std::cend(fields));
   }
   
   auto expire(std::string const& key, int secs)
   {
      auto par = {std::to_string(secs)};
      resp::assemble(payload, "EXPIRE", {key}, std::cbegin(par), std::cend(par));
   }
   
   auto zadd(std::string const& key, int score, std::string const& value)
   {
      auto par = {std::to_string(score), value};
      resp::assemble(payload, "ZADD", {key}, std::cbegin(par), std::cend(par));
   }
   
   template <class Key, class T, class Compare, class Allocator>
   auto zadd( std::initializer_list<std::string> key
            , std::map<Key, T, Compare, Allocator> const& m)
     { resp::assemble(payload, "ZADD", key, std::cbegin(m), std::cend(m), 2); }
   
   auto zrange(std::string const& key, int min = 0, int max = -1)
   {
      auto par = {std::to_string(min), std::to_string(max)};
      resp::assemble(payload, "ZRANGE", {key}, std::cbegin(par), std::cend(par));
   }
   
   auto zrangebyscore(std::string const& key, int min, int max)
   {
      auto max_str = std::string {"inf"};
      if (max != -1)
         max_str = std::to_string(max);
   
      auto par = {std::to_string(min) , max_str};
      resp::assemble(payload, "zrangebyscore", {key}, std::cbegin(par), std::cend(par));
   }
   
   auto zremrangebyscore(std::string const& key, int score)
   {
      auto const s = std::to_string(score);
      auto par = {s, s};
      resp::assemble(payload, "ZREMRANGEBYSCORE", {key}, std::cbegin(par), std::cend(par));
   }
   
   auto lrange(std::string const& key, int min = 0, int max = -1)
   {
      auto par = { std::to_string(min) , std::to_string(max) };
      resp::assemble(payload, "LRANGE", {key}, std::cbegin(par), std::cend(par));
   }
   
   auto ltrim(std::string const& key, int min = 0, int max = -1)
   {
      auto par = { std::to_string(min) , std::to_string(max) };
      resp::assemble(payload, "LTRIM", {key}, std::cbegin(par), std::cend(par));
   }
   
   auto del(std::string const& key)
     { resp::assemble(payload, "DEL", key); }
   
   auto llen(std::string const& key)
     { resp::assemble(payload, "LLEN", key); }

   template <class Iter>
   void sadd(std::string const& key, Iter begin, Iter end)
     { resp::assemble(payload, "SADD", {key}, begin, end); }

   template <class Range>
   void sadd(std::string const& key, Range const& r)
   {
      using std::cbegin;
      using std::cend;
      sadd(key, cbegin(r), cend(r));
   }
};

}

}

