/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/assert.hpp>
#include <aedis/resp3/type.hpp>

namespace aedis {
namespace resp3 {

char const* to_string(type t)
{
   static char const* table[] =
   { "array"
   , "push"
   , "set"
   , "map"
   , "attribute"
   , "simple_string"
   , "simple_error"
   , "number"
   , "doublean"
   , "boolean"
   , "big_number"
   , "null"
   , "blob_error"
   , "verbatim_string"
   , "blob_string"
   , "streamed_string_part"
   , "invalid"
   };

   return table[static_cast<int>(t)];
}

std::ostream& operator<<(std::ostream& os, type t)
{
   os << to_string(t);
   return os;
}

bool is_aggregate(type t)
{
   switch (t) {
      case type::array:
      case type::push:
      case type::set:
      case type::map:
      case type::attribute: return true;
      default: return false;
   }
}

std::size_t element_multiplicity(type t)
{
   switch (t) {
      case type::map:
      case type::attribute: return 2ULL;
      default: return 1ULL;
   }
}

char to_code(type t)
{
   switch (t) {
      case type::blob_error:           return '!';
      case type::verbatim_string:      return '=';
      case type::blob_string:          return '$';
      case type::streamed_string_part: return ';';
      case type::simple_error:         return '-';
      case type::number:               return ':';
      case type::doublean:             return ',';
      case type::boolean:              return '#';
      case type::big_number:           return '(';
      case type::simple_string:        return '+';
      case type::null:                 return '_';
      case type::push:                 return '>';
      case type::set:                  return '~';
      case type::array:                return '*';
      case type::attribute:            return '|';
      case type::map:                  return '%';

      default:
      BOOST_ASSERT(false);
      return ' ';
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
      case '>': return type::push;
      case '~': return type::set;
      case '*': return type::array;
      case '|': return type::attribute;
      case '%': return type::map;
      default: return type::invalid;
   }
}

} // resp3
} // aedis
