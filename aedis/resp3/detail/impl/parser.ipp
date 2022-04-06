/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres at gmail dot com)
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

std::size_t parse_uint(char const* data, std::size_t size, boost::system::error_code& ec)
{
   static constexpr boost::spirit::x3::uint_parser<std::size_t, 10> p{};
   std::size_t ret;
   if (!parse(data, data + size, p, ret))
      ec = error::not_a_number;

   return ret;
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

} // detail
} // resp3
} // aedis
