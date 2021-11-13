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


type to_type(char c)
{
   switch (c) {
      case '!': return type::blob_error;
      case '=': return type::verbatim_string;
      case '$': return type::blob_string;
      case ';': return type::streamed_string_part;
      case '-': return type::simple_error;
      case ':': return type::number;
      case ',': return type::doublean;
      case '#': return type::boolean;
      case '(': return type::big_number;
      case '+': return type::simple_string;
      case '_': return type::null;
      case '>': return type::push;
      case '~': return type::set;
      case '*': return type::array;
      case '|': return type::attribute;
      case '%': return type::map;
      default: return type::invalid;
   }
}

// Converts a decimal number in ascii format to an integer.
std::size_t length(char const* p)
{
   std::size_t len = 0;
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
   bulk_ = type::invalid;
   bulk_length_ = std::numeric_limits<std::size_t>::max();
}

std::size_t parser::advance(char const* data, std::size_t n)
{
   if (bulk_ != type::invalid) {
      n = bulk_length_ + 2;
      switch (bulk_) {
         case type::streamed_string_part:
         {
           if (bulk_length_ == 0) {
              sizes_[depth_] = 1;
           } else {
              res_->add(bulk_, 1, depth_, data, bulk_length_);
           }
         } break;
         default: res_->add(bulk_, 1, depth_, data, bulk_length_);
      }

      bulk_ = type::invalid;
      --sizes_[depth_];

   } else if (sizes_[depth_] != 0) {
      auto const t = to_type(*data);
      switch (t) {
         case type::blob_error:
         case type::verbatim_string:
         case type::streamed_string_part:
         {
            bulk_length_ = length(data + 1);
            bulk_ = t;
         } break;
         case type::blob_string:
         {
            if (*(data + 1) == '?') {
               sizes_[++depth_] = std::numeric_limits<std::size_t>::max();
            } else {
               bulk_length_ = length(data + 1);
               bulk_ = type::blob_string;
            }
         } break;
         case type::simple_error:
         case type::number:
         case type::doublean:
         case type::boolean:
         case type::big_number:
         case type::simple_string:
         {
            res_->add(t, 1, depth_, data + 1, n - 3);
            --sizes_[depth_];
         } break;
         case type::null:
         {
            res_->add(type::null, 1, depth_);
            --sizes_[depth_];
         } break;
         case type::push:
         case type::set:
         case type::array:
         case type::attribute:
         case type::map:
         {
            auto const l = length(data + 1);
            res_->add(t, l, depth_);

            if (l == 0) {
               --sizes_[depth_];
            } else {
               auto const m = element_multiplicity(t);
               sizes_[++depth_] = m * l;
            }
         } break;
         default:
         {
            // TODO: This should cause an error not an assert.
            assert(false);
         }
      }
   }
   
   while (sizes_[depth_] == 0)
      --sizes_[--depth_];
   
   return n;
}

} // detail
} // resp3
} // aedis
