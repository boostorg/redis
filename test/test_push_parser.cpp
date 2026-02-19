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
#include <optional>
#include <vector>

using namespace boost::redis;
using detail::tree_from_resp3;

// Operators
namespace boost::redis {

std::ostream& operator<<(std::ostream& os, const push_view& v)
{
   os << "push_view { .channel=" << v.channel;
   if (v.pattern)
      os << ", .pattern=" << *v.pattern;
   return os << ", .payload=" << v.payload << " }";
}

bool operator==(const push_view& lhs, const push_view& rhs) noexcept
{
   return lhs.channel == rhs.channel && lhs.pattern == rhs.pattern && lhs.payload == rhs.payload;
}

}  // namespace boost::redis

namespace {

// --- Only valid messages ---
void test_one_message()
{
   auto nodes = tree_from_resp3({">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n"});
   push_parser p{nodes};

   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

void test_several_messages()
{
   auto nodes = tree_from_resp3({
      ">3\r\n$7\r\nmessage\r\n$5\r\nfirst\r\n$2\r\nHi\r\n",
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$3\r\nBye\r\n",
   });
   push_parser p{nodes};

   const push_view expected[] = {
      {"first",  std::nullopt, "Hi" },
      {"second", std::nullopt, "Bye"},
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

void test_one_pmessage()
{
   auto nodes = tree_from_resp3(
      {">4\r\n$8\r\npmessage\r\n$6\r\nmycha*\r\n$6\r\nmychan\r\n$5\r\nHello\r\n"});
   push_parser p{nodes};

   const push_view expected[] = {
      {"mychan", "mycha*", "Hello"},
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

void test_messages_pmessages()
{
   auto nodes = tree_from_resp3({
      ">3\r\n$7\r\nmessage\r\n$3\r\nch1\r\n$4\r\nmsg1\r\n",
      ">4\r\n$8\r\npmessage\r\n$1\r\n*\r\n$3\r\nch2\r\n$4\r\nmsg2\r\n",
      ">3\r\n$7\r\nmessage\r\n$3\r\nch3\r\n$4\r\nmsg3\r\n",
   });
   push_parser p{nodes};

   const push_view expected[] = {
      {"ch1", std::nullopt, "msg1"},
      {"ch2", "*",          "msg2"},
      {"ch3", std::nullopt, "msg3"},
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

void test_message_empty_fields()
{
   auto nodes = tree_from_resp3({">3\r\n$7\r\nmessage\r\n$0\r\n\r\n$0\r\n\r\n"});
   push_parser p{nodes};

   const push_view expected[] = {
      {"", std::nullopt, ""},
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

void test_pmessage_empty_fields()
{
   auto nodes = tree_from_resp3({">4\r\n$8\r\npmessage\r\n$0\r\n\r\n$0\r\n\r\n$0\r\n\r\n"});
   push_parser p{nodes};

   const push_view expected[] = {
      {"", "", ""},
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

// --- Message skipping (valid, expected messages we don't care about) ---

// Pushes in RESP2 used to be arrays. We don't support these
void test_skip_resp2_push()
{
   auto nodes = tree_from_resp3({
      "*3\r\n$7\r\nmessage\r\n$5\r\nfirst\r\n$5\r\nValue\r\n",
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n",
   });
   push_parser p{nodes};

   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

// --- Edge cases ---
void test_empty()
{
   auto nodes = tree_from_resp3({});
   push_parser p{nodes};
   std::vector<push_view> expected;

   BOOST_TEST_ALL_EQ(p.begin(), p.end(), expected.begin(), expected.end());
}

}  // namespace

int main()
{
   test_one_message();
   test_several_messages();
   test_one_pmessage();
   test_messages_pmessages();
   test_message_empty_fields();
   test_pmessage_empty_fields();

   test_skip_resp2_push();

   test_empty();

   return boost::report_errors();
}
