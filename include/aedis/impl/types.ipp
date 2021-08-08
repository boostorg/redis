/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/types.hpp>

#include <cassert>

namespace aedis {

#define EXPAND_TYPE_CASE(x) case types::x: return #x

std::string to_string(types type)
{
   switch (type) {
      EXPAND_TYPE_CASE(array);
      EXPAND_TYPE_CASE(push);
      EXPAND_TYPE_CASE(set);
      EXPAND_TYPE_CASE(map);
      EXPAND_TYPE_CASE(attribute);
      EXPAND_TYPE_CASE(simple_string);
      EXPAND_TYPE_CASE(simple_error);
      EXPAND_TYPE_CASE(number);
      EXPAND_TYPE_CASE(double_type);
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

types to_type(char c)
{
   switch (c) {
      case '!': return types::blob_error;
      case '=': return types::verbatim_string;
      case '$': return types::blob_string;
      case ';': return types::streamed_string_part;
      case '-': return types::simple_error;
      case ':': return types::number;
      case ',': return types::double_type;
      case '#': return types::boolean;
      case '(': return types::big_number;
      case '+': return types::simple_string;
      case '_': return types::null;
      case '>': return types::push;
      case '~': return types::set;
      case '*': return types::array;
      case '|': return types::attribute;
      case '%': return types::map;
      default: return types::invalid;
   }
}

} // aedis

std::ostream& operator<<(std::ostream& os, aedis::types type)
{
   os << to_string(type);
   return os;
}
