/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/resp3/type.hpp>
#include <boost/assert.hpp>

namespace boost::redis::resp3 {

auto to_string(type t) -> char const*
{
   switch (t) {
      case type::array: return "array";
      case type::push: return "push";
      case type::set: return "set";
      case type::map: return "map";
      case type::attribute: return "attribute";
      case type::simple_string: return "simple_string";
      case type::simple_error: return "simple_error";
      case type::number: return "number";
      case type::doublean: return "doublean";
      case type::boolean: return "boolean";
      case type::big_number: return "big_number";
      case type::null: return "null";
      case type::blob_error: return "blob_error";
      case type::verbatim_string: return "verbatim_string";
      case type::blob_string: return "blob_string";
      case type::streamed_string: return "streamed_string";
      case type::streamed_string_part: return "streamed_string_part";
      default: return "invalid";
   }
}

auto operator<<(std::ostream& os, type t) -> std::ostream&
{
   os << to_string(t);
   return os;
}

auto is_aggregate(type t) -> bool
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

auto element_multiplicity(type t) -> std::size_t
{
   switch (t) {
      case type::map:
      case type::attribute: return 2ULL;
      default: return 1ULL;
   }
}

auto to_code(type t) -> char
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

      default: BOOST_ASSERT(false); return ' ';
   }
}

auto to_type(char c) -> type
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

} // boost::redis::resp3
