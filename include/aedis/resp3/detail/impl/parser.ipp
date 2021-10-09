/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/resp3/detail/parser.hpp>
#include <aedis/resp3/type.hpp>

namespace aedis {
namespace resp3 {
namespace detail {

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

parser::parser(response_adapter_base* res)
   { init(res); }

void parser::init(response_adapter_base* res)
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
   res_->add(type::null);
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
      case bulk_type::blob_error: res_->add(type::blob_error, s); break;
      case bulk_type::verbatim_string: res_->add(type::verbatim_string, s); break;
      case bulk_type::blob_string: res_->add(type::blob_string, s); break;
      case bulk_type::streamed_string_part:
      {
	if (std::empty(s)) {
	   sizes_[depth_] = 1;
	} else {
	   res_->add(type::streamed_string_part, s);
	}
      } break;
      default: assert(false);
   }

   --sizes_[depth_];
}

parser::bulk_type parser::on_blob_error(char const* data, parser::bulk_type b)
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

   return on_blob_error(data, bulk_type::blob_string);
}

void parser::on_data(type t, char const* data, std::size_t n)
{
   auto const sv = handle_simple_string(data, n);
   res_->add(t, sv);
}

void parser::on_aggregate(type t, char const* data)
{
   int counter;
   switch (t) {
      case type::map:
      case type::attribute:
      {
	 counter = 2;
      } break;
      default: counter = 1;
   }

   res_->add_aggregate(t, on_array_impl(data, counter));
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
	    case '!': next = on_blob_error(data, bulk_type::blob_error); break;
	    case '=': next = on_blob_error(data, bulk_type::verbatim_string); break; 
	    case '$': next = on_blob_string(data); break;
	    case ';': next = on_blob_error(data, bulk_type::streamed_string_part); break;
	    case '-': on_data(type::simple_error, data, n); break;
	    case ':': on_data(type::number, data, n); break;
	    case ',': on_data(type::doublean, data, n); break;
	    case '#': on_data(type::boolean, data, n); break;
	    case '(': on_data(type::big_number, data, n); break;
	    case '+': on_data(type::simple_string, data, n); break;
	    case '_': on_null(); break;
	    case '>': on_aggregate(type::push, data); break;
	    case '~': on_aggregate(type::set, data); break;
	    case '*': on_aggregate(type::array, data); break;
	    case '|': on_aggregate(type::attribute, data); break;
	    case '%': on_aggregate(type::map, data); break;
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

} // detail
} // resp3
} // aedis
