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

// Anything that is not a RESP3 push is skipped.
// Concretely, pushes in RESP2 used to be arrays. We don't support these
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

// Might happen if a SUBSCRIBE call fails
void test_skip_simple_error()
{
   auto nodes = tree_from_resp3({
      "-ERR foo\r\n",
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n",
   });
   push_parser p{nodes};

   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

// Might happen when using MONITOR
void test_skip_simple_string()
{
   auto nodes = tree_from_resp3({
      "+MONITOR output\r\n",
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n",
   });
   push_parser p{nodes};

   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

// Edge case: other RESP3 types are skipped
void test_skip_other_message_type()
{
   auto nodes = tree_from_resp3({
      "%1\r\n$3\r\nkey\r\n$5\r\nvalue\r\n",
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n",
   });
   push_parser p{nodes};

   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

// The push first field is not 'message' or 'pmessage' (e.g. subscribe confirmations)
void test_skip_subscribe_confirmations()
{
   auto nodes = tree_from_resp3({
      ">3\r\n$9\r\nsubscribe\r\n$6\r\nmychan\r\n:1\r\n",
      ">3\r\n$11\r\nunsubscribe\r\n$6\r\nmychan\r\n:1\r\n",
      ">3\r\n$10\r\npsubscribe\r\n$5\r\np*tt*\r\n:1\r\n",
      ">3\r\n$12\r\npunsubscribe\r\n$5\r\np*tt*\r\n:1\r\n",
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n",
   });
   push_parser p{nodes};

   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

// Message type string, but with an unknown value (not 'message' or 'pmessage')
void test_skip_unknown_message_type()
{
   auto nodes = tree_from_resp3({
      ">3\r\n$7\r\nunknown\r\n$4\r\nchan\r\n$4\r\nbody\r\n",
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n",
   });
   push_parser p{nodes};

   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

// Message type is an empty string
void test_skip_empty_message_type()
{
   auto nodes = tree_from_resp3({
      ">3\r\n$0\r\n\r\n$4\r\nchan\r\n$4\r\nbody\r\n",
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n",
   });
   push_parser p{nodes};

   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

// Message type is not a string (e.g. array)
void test_skip_non_string_message_type()
{
   auto nodes = tree_from_resp3({
      ">3\r\n*1\r\n$3\r\nfoo\r\n$5\r\nHello\r\n$5\r\nworld\r\n",
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n",
   });
   push_parser p{nodes};

   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

// Push has type 'message'/'pmessage' but wrong push size
void test_skip_message_size_1()
{
   auto nodes = tree_from_resp3({
      ">1\r\n$7\r\nmessage\r\n",
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n",
   });
   push_parser p{nodes};
   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

void test_skip_message_push_size_2()
{
   auto nodes = tree_from_resp3({
      ">2\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n",
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n",
   });
   push_parser p{nodes};
   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

void test_skip_message_push_size_4()
{
   auto nodes = tree_from_resp3({
      ">4\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n$1\r\nx\r\n",
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n",
   });
   push_parser p{nodes};
   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

void test_skip_pmessage_push_size_1()
{
   auto nodes = tree_from_resp3({
      ">1\r\n$8\r\npmessage\r\n",
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n",
   });
   push_parser p{nodes};
   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

void test_skip_pmessage_push_size_3()
{
   auto nodes = tree_from_resp3({
      ">3\r\n$8\r\npmessage\r\n$1\r\n*\r\n$3\r\nch2\r\n",
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n",
   });
   push_parser p{nodes};
   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

void test_skip_pmessage_push_size_5()
{
   auto nodes = tree_from_resp3({
      ">5\r\n$8\r\npmessage\r\n$1\r\n*\r\n$3\r\nch2\r\n$4\r\nmsg2\r\n$1\r\nx\r\n",
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
   test_skip_simple_error();
   test_skip_simple_string();
   test_skip_other_message_type();
   test_skip_subscribe_confirmations();
   test_skip_unknown_message_type();
   test_skip_empty_message_type();
   test_skip_non_string_message_type();
   test_skip_message_size_1();
   test_skip_message_push_size_2();
   test_skip_message_push_size_4();
   test_skip_pmessage_push_size_1();
   test_skip_pmessage_push_size_3();
   test_skip_pmessage_push_size_5();

   test_empty();

   return boost::report_errors();
}
