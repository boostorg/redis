//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/adapter/any_adapter.hpp>
#include <boost/redis/resp3/flat_tree.hpp>
#include <boost/redis/resp3/node.hpp>
#include <boost/redis/resp3/parser.hpp>
#include <boost/redis/resp3/type.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/system/detail/error_code.hpp>
#include <boost/system/error_code.hpp>

#include "print_node.hpp"

#include <iostream>
#include <string_view>
#include <vector>

using namespace boost::redis;
using boost::system::error_code;
using resp3::type;

namespace {

#define S01a "#11\r\n"
#define S01b "#f\r\n"
#define S01c "#t\r\n"
#define S01d "#\r\n"

#define S02a "$?\r\n;0\r\n"
#define S02b "$?\r\n;4\r\nHell\r\n;5\r\no wor\r\n;1\r\nd\r\n;0\r\n"
#define S02c "$?\r\n;b\r\nHell\r\n;5\r\no wor\r\n;1\r\nd\r\n;0\r\n"
#define S02d "$?\r\n;d\r\nHell\r\n;5\r\no wor\r\n;1\r\nd\r\n;0\r\n"

#define S03a "%11\r\n"
#define S03b                                                                           \
   "%4\r\n$4\r\nkey1\r\n$6\r\nvalue1\r\n$4\r\nkey2\r\n$6\r\nvalue2\r\n$4\r\nkey3\r\n$" \
   "6\r\nvalue3\r\n$4\r\nkey3\r\n$6\r\nvalue3\r\n"
#define S03c "%0\r\n"
#define S03d "%rt\r\n$4\r\nkey1\r\n$6\r\nvalue1\r\n$4\r\nkey2\r\n$6\r\nvalue2\r\n"

#define S04a "*1\r\n:11\r\n"
#define S04b "*3\r\n$2\r\n11\r\n$2\r\n22\r\n$1\r\n3\r\n"
#define S04c "*1\r\n" S03b
#define S04d "*1\r\n" S09a
#define S04e "*3\r\n$2\r\n11\r\n$2\r\n22\r\n$1\r\n3\r\n"
#define S04f "*1\r\n*1\r\n$2\r\nab\r\n"
#define S04g "*1\r\n*1\r\n*1\r\n*1\r\n*1\r\n*1\r\na\r\n"
#define S04h "*0\r\n"
#define S04i "*3\r\n$2\r\n11\r\n$2\r\n22\r\n$1\r\n3\r\n"

#define S05a ":-3\r\n"
#define S05b ":11\r\n"
#define s05c ":3\r\n"
#define S05d ":adf\r\n"
#define S05e ":\r\n"

#define S06a "_\r\n"

#define S07a ">4\r\n+pubsub\r\n+message\r\n+some-channel\r\n+some message\r\n"
#define S07b ">0\r\n"

#define S08a "|1\r\n+key-popularity\r\n%2\r\n$1\r\na\r\n,0.1923\r\n$1\r\nb\r\n,0.0012\r\n"
#define S08b "|0\r\n"

#define S09a "~6\r\n+orange\r\n+apple\r\n+one\r\n+two\r\n+three\r\n+orange\r\n"
#define S09b "~0\r\n"

#define S10a "-Error\r\n"
#define S10b "-\r\n"

#define S11a ",1.23\r\n"
#define S11b ",inf\r\n"
#define S11c ",-inf\r\n"
#define S11d ",1.23\r\n"
#define S11e ",er\r\n"
#define S11f ",\r\n"

#define S12a "!21\r\nSYNTAX invalid syntax\r\n"
#define S12b "!0\r\n\r\n"
#define S12c "!3\r\nfoo\r\n"

#define S13a "=15\r\ntxt:Some string\r\n"
#define S13b "=0\r\n\r\n"

#define S14a "(3492890328409238509324850943850943825024385\r\n"
#define S14b "(\r\n"

#define S15a "+OK\r\n"
#define S15b "+\r\n"

#define S16a "s11\r\n"

#define S17a "$l\r\nhh\r\n"
#define S17b "$2\r\nhh\r\n"
#define S18c "$26\r\nhhaa\aaaa\raaaaa\r\naaaaaaaaaa\r\n"
#define S18d "$0\r\n\r\n"

void test_success()
{
   const struct {
      std::string_view name;
      std::string_view input;
      std::vector<resp3::node_view> expected;
   } test_cases[] = {
      // clang-format off
      { "boolean_false", "#f\r\n", {
         { type::boolean, 1u, 0u, "f" },
      } },
      { "boolean_true", "#t\r\n", {
         { type::boolean, 1u, 0u, "t" },
      } },
      // clang-format on
   };

   for (const auto& tc : test_cases) {
      std::cerr << "Running test case " << tc.name << std::endl;

      resp3::parser p;
      resp3::flat_tree tree;
      any_adapter adapter{tree};
      error_code ec;
      bool done = resp3::parse(p, tc.input, adapter, ec);
      BOOST_TEST(done);
      BOOST_TEST(p.done());
      BOOST_TEST_EQ(p.get_consumed(), tc.input.size());
      BOOST_TEST_EQ(ec, error_code());
      BOOST_TEST_ALL_EQ(
         tree.get_view().begin(),
         tree.get_view().end(),
         tc.expected.begin(),
         tc.expected.end());
   }
}

}  // namespace

int main()
{
   test_success();

   return boost::report_errors();
}
