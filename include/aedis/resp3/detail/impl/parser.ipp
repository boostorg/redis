/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/home/x3.hpp>

#include <aedis/resp3/detail/parser.hpp>
#include <aedis/resp3/type.hpp>

namespace aedis {
namespace resp3 {
namespace detail {

auto parse_uint(char const* data, std::size_t size, boost::system::error_code& ec) -> std::size_t
{
   static constexpr boost::spirit::x3::uint_parser<std::size_t, 10> p{};
   std::size_t ret = 0;
   if (!parse(data, data + size, p, ret))
      ec = error::not_a_number;

   return ret;
}

} // detail
} // resp3
} // aedis
