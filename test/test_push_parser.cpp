//
// Copyright (c) 2026 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/push_parser.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/system/error_code.hpp>

#include "sansio_utils.hpp"

#include <iterator>

using namespace boost::redis;
using detail::tree_from_resp3;

// Operators
namespace boost::redis {

std::ostream& operator<<(std::ostream& os, const push_view& v)
{
   return os << "push_view { .channel=" << v.channel << ", .pattern=" << v.pattern
             << ", .payload=" << v.payload << " }";
}

bool operator==(const push_view& lhs, const push_view& rhs) noexcept
{
   return lhs.channel == rhs.channel && lhs.pattern == rhs.pattern && lhs.payload == rhs.payload;
}

}  // namespace boost::redis

namespace {

void test_one_message()
{
   auto nodes = tree_from_resp3({">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n"});

   constexpr push_view expected[] = {
      {"second", "", "Hello"}
   };
   push_parser p{nodes};

   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

}  // namespace

int main()
{
   test_one_message();

   return boost::report_errors();
}
