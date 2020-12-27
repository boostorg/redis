/* Copyright (c) 2019 - 2020 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

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

namespace aedis
{

namespace net = boost::asio;
namespace ip = net::ip;
using tcp = ip::tcp;

namespace resp
{

// Converts a decimal number in ascii format to an integer.
inline
long long length(char const* p)
{
   long long len = 0;
   while (*p != '\r') {
       len = (10 * len) + (*p - '0');
       p++;
   }
   return len;
}

enum class bulk_type
{ blob_error
, verbatim_string 
, blob_string
, streamed_string_part
, none
};

template <class Response>
class parser {
public:
private:
   Response* res_;
   int depth_;
   int sizes_[6]; // Streaming will require a bigger integer.
   bulk_type bulk_;
   int bulk_length_;

   void init(Response* res)
   {
      res_ = res;
      depth_ = 0;
      sizes_[0] = 2;
      sizes_[1] = 1;
      sizes_[2] = 1;
      sizes_[3] = 1;
      sizes_[4] = 1;
      sizes_[5] = 1;
      sizes_[6] = 1;
      bulk_ = bulk_type::none;
      bulk_length_ = std::numeric_limits<int>::max();
   }

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
   {
      res_->on_null();
      --sizes_[depth_];
   }

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

   void on_bulk(bulk_type b, std::string_view s = {})
   {
      switch (b) {
	 case bulk_type::blob_error: res_->on_blob_error(s); break;
	 case bulk_type::verbatim_string: res_->on_verbatim_string(s); break;
	 case bulk_type::blob_string: res_->on_blob_string(s); break;
	 case bulk_type::streamed_string_part:
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

   auto on_blob_error_impl(char const* data, bulk_type b)
   {
      bulk_length_ = length(data + 1);
      return b;
   }

   auto on_streamed_string_size(char const* data)
      { return on_blob_error_impl(data, bulk_type::streamed_string_part); }

   auto on_blob_error(char const* data)
      { return on_blob_error_impl(data, bulk_type::blob_error); }

   auto on_verbatim_string(char const* data)
      { return on_blob_error_impl(data, bulk_type::verbatim_string); }

   auto on_blob_string(char const* data)
   {
      if (*(data + 1) == '?') {
	 sizes_[++depth_] = std::numeric_limits<int>::max();
	 return bulk_type::none;
      }

      return on_blob_error_impl(data, bulk_type::blob_string);
   }

public:
   parser(Response* res)
   { init(res); }

   std::size_t advance(char const* data, std::size_t n)
   {
      auto next = bulk_type::none;
      if (bulk_ != bulk_type::none) {
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
         } else {
	 }
      }
      
      while (sizes_[depth_] == 0) {
	 res_->pop();
         --sizes_[--depth_];
      }
      
      bulk_ = next;
      return n;
   }

   auto done() const noexcept
     { return depth_ == 0 && bulk_ == bulk_type::none; }

   auto bulk() const noexcept
     { return bulk_; }

   auto bulk_length() const noexcept
     { return bulk_length_; }
};

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
template <
  class AsyncReadStream,
  class Storage,
  class Response>
class parse_op {
private:
   AsyncReadStream& stream_;
   Storage* buf_ = nullptr;
   parser<Response> parser_;
   int start_ = 1;

public:
   parse_op(AsyncReadStream& stream, Storage* buf, Response* res)
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
            if (parser_.bulk() == bulk_type::none) {
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
	       auto const to_read = static_cast<std::size_t>(l + 2 - s);
               buf_->resize(l + 2);
               net::async_read(
                  stream_,
                  net::buffer(buf_->data() + s, to_read),
                  net::transfer_all(),
                  std::move(self));
               return;
            }

            default:
	    {
	       if (ec)
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

template <
   class SyncReadStream,
   class Storage,
   class Response>
auto read(
   SyncReadStream& stream,
   Storage& buf,
   Response& res,
   boost::system::error_code& ec)
{
   parser<Response> p {&res};
   std::size_t n = 0;
   do {
      if (p.bulk() == bulk_type::none) {
	 n = net::read_until(stream, net::dynamic_buffer(buf), "\r\n", ec);
	 if (ec || n < 3)
	    return n;
      } else {
	 auto const s = std::ssize(buf);
	 auto const l = p.bulk_length();
	 if (s < (l + 2)) {
	    buf.resize(l + 2);
	    auto const to_read = static_cast<std::size_t>(l + 2 - s);
	    n = net::read(stream, net::buffer(buf.data() + s, to_read));
	    assert(n >= to_read);
	    if (ec)
	       return n;
	 }
      }

      n = p.advance(buf.data(), n);
      buf.erase(0, n);
   } while (!p.done());

   return n;
}

template<
   class SyncReadStream,
   class Storage,
   class Response>
std::size_t
read(
   SyncReadStream& stream,
   Storage& buf,
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
   class Storage,
   class Response,
   class CompletionToken =
      net::default_completion_token_t<typename AsyncReadStream::executor_type>
   >
auto async_read(
   AsyncReadStream& stream,
   Storage& buffer,
   Response& res,
   CompletionToken&& token =
      net::default_completion_token_t<typename AsyncReadStream::executor_type>{})
{
   return net::async_compose
      < CompletionToken
      , void(boost::system::error_code)
      >(parse_op<AsyncReadStream, Storage, Response> {stream, &buffer, &res},
        token,
        stream);
}

} // resp
} // aedis
