/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "parser.hpp"

//#include <string>
//#include <cstdio>
//#include <cstring>
//#include <numeric>
//#include <type_traits>
//#include <charconv>

namespace aedis { namespace resp {

// Converts a decimal number in ascii format to an integer.
long long length(char const* p)
{
   long long len = 0;
   while (*p != '\r') {
       len = (10 * len) + (*p - '0');
       p++;
   }
   return len;
}

parser::parser(response_base* res)
   { init(res); }

void parser::init(response_base* res)
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

long long parser::on_array_impl(char const* data, int m)
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

void parser::on_null()
{
   res_->on_null();
   --sizes_[depth_];
}

std::string_view
parser::handle_simple_string(char const* data, std::size_t n)
{
   --sizes_[depth_];
   return std::string_view {data + 1, n - 3};
}

void parser::on_bulk(parser::bulk_type b, std::string_view s)
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

parser::bulk_type parser::on_blob_error_impl(char const* data, parser::bulk_type b)
{
   bulk_length_ = length(data + 1);
   return b;
}

parser::bulk_type parser::on_blob_string(char const* data)
{
   if (*(data + 1) == '?') {
      sizes_[++depth_] = std::numeric_limits<int>::max();
      return bulk_type::none;
   }

   return on_blob_error_impl(data, bulk_type::blob_string);
}

std::size_t parser::advance(char const* data, std::size_t n)
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

} // resp
} // aedis
