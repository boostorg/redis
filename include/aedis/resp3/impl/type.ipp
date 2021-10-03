/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/resp3/type.hpp>

#include <cassert>

namespace aedis { namespace resp3 {

bool operator==(node const& a, node const& b)
{
   return a.size == b.size
       && a.depth == b.depth
       && a.data_type == b.data_type
       && a.data == b.data;
};

#define EXPAND_TYPE_CASE(x) case resp3::type::x: return #x

std::string to_string(type t)
{
   switch (t) {
      EXPAND_TYPE_CASE(array);
      EXPAND_TYPE_CASE(flat_array);
      EXPAND_TYPE_CASE(flat_push);
      EXPAND_TYPE_CASE(flat_set);
      EXPAND_TYPE_CASE(flat_map);
      EXPAND_TYPE_CASE(flat_attribute);
      EXPAND_TYPE_CASE(simple_string);
      EXPAND_TYPE_CASE(simple_error);
      EXPAND_TYPE_CASE(number);
      EXPAND_TYPE_CASE(doublean);
      EXPAND_TYPE_CASE(boolean);
      EXPAND_TYPE_CASE(big_number);
      EXPAND_TYPE_CASE(null);
      EXPAND_TYPE_CASE(blob_error);
      EXPAND_TYPE_CASE(verbatim_string);
      EXPAND_TYPE_CASE(blob_string);
      EXPAND_TYPE_CASE(streamed_string_part);
      EXPAND_TYPE_CASE(invalid);
      default: assert(false);
   }
}

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
      case '>': return type::flat_push;
      case '~': return type::flat_set;
      case '*': return type::flat_array;
      case '|': return type::flat_attribute;
      case '%': return type::flat_map;
      default: return type::invalid;
   }
}

} // resp3
} // aedis

std::ostream& operator<<(std::ostream& os, aedis::resp3::type t)
{
   os << to_string(t);
   return os;
}
