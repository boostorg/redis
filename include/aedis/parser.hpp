/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <string>
#include <cstdio>
#include <cstring>
#include <numeric>
#include <type_traits>
#include <charconv>

#include "type.hpp"

namespace aedis { namespace resp {

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

} // resp
} // aedis
