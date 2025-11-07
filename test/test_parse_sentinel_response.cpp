//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/adapter/any_adapter.hpp>
#include <boost/redis/config.hpp>
#include <boost/redis/detail/connection_state.hpp>
#include <boost/redis/impl/parse_sentinel_response.hpp>
#include <boost/redis/resp3/node.hpp>
#include <boost/redis/resp3/parser.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/system/detail/error_code.hpp>
#include <boost/system/error_code.hpp>

#include <iterator>
#include <ostream>
#include <vector>

using namespace boost::redis;
using detail::parse_sentinel_response;
using detail::sentinel_response;
using boost::system::error_code;

// Operators
namespace boost::redis {

bool operator==(const address& lhs, const address& rhs)
{
   return lhs.host == rhs.host && lhs.port == rhs.port;
}

std::ostream& operator<<(std::ostream& os, const address& addr)
{
   return os << "address{ .host=" << addr.host << ", .port=" << addr.port << " }";
}

}  // namespace boost::redis

namespace {

void test_success()
{
   // Setup
   sentinel_response resp{
      "leftover",
      {"leftover_host", "6543"},
      {address()}
   };
   std::vector<resp3::node> nodes;
   auto adapter = detail::make_vector_adapter(nodes);

   // Load the vector. Using the raw RESP values ensures that the correct
   // node tree is built, which is not always obvious
   const char* const responses[] = {
      // clang-format off
      "*2\r\n$9\r\nlocalhost\r\n$4\r\n6380\r\n",

      "*2\r\n"
         "%14\r\n"
            "$4\r\nname\r\n$40\r\nf14ef06a8a478cdd66ded467ec18accd2a24b731\r\n$2\r\nip\r\n$8\r\nhost.one\r\n$4\r\nport\r\n$5\r\n26380\r\n"
            "$5\r\nrunid\r\n$40\r\nf14ef06a8a478cdd66ded467ec18accd2a24b731\r\n$5\r\nflags\r\n$8\r\nsentinel\r\n"
            "$21\r\nlink-pending-commands\r\n$1\r\n0\r\n$13\r\nlink-refcount\r\n$1\r\n1\r\n$14\r\nlast-ping-sent\r\n$1\r\n0\r\n"
            "$18\r\nlast-ok-ping-reply\r\n$3\r\n696\r\n$15\r\nlast-ping-reply\r\n$3\r\n696\r\n$23\r\ndown-after-milliseconds\r\n$5\r\n10000\r\n"
            "$18\r\nlast-hello-message\r\n$3\r\n334\r\n$12\r\nvoted-leader\r\n$1\r\n?\r\n$18\r\nvoted-leader-epoch\r\n$1\r\n0\r\n"
         "%14\r\n"
            "$4\r\nname\r\n$40\r\nf9b54e79e2e7d3f17ad60527504191ec8a861f27\r\n$2\r\nip\r\n$8\r\nhost.two\r\n$4\r\nport\r\n$5\r\n26381\r\n"
            "$5\r\nrunid\r\n$40\r\nf9b54e79e2e7d3f17ad60527504191ec8a861f27\r\n$5\r\nflags\r\n$8\r\nsentinel\r\n"
            "$21\r\nlink-pending-commands\r\n$1\r\n0\r\n$13\r\nlink-refcount\r\n$1\r\n1\r\n$14\r\nlast-ping-sent\r\n$1\r\n0\r\n"
            "$18\r\nlast-ok-ping-reply\r\n$3\r\n696\r\n$15\r\nlast-ping-reply\r\n$3\r\n696\r\n$23\r\ndown-after-milliseconds\r\n$5\r\n10000\r\n"
            "$18\r\nlast-hello-message\r\n$3\r\n134\r\n$12\r\nvoted-leader\r\n$1\r\n?\r\n$18\r\nvoted-leader-epoch\r\n$1\r\n0\r\n",
      // clang-format on
   };

   for (const char* resp : responses) {
      resp3::parser p;
      error_code ec;
      bool ok = resp3::parse(p, resp, adapter, ec);
      BOOST_TEST(ok);
      BOOST_TEST_EQ(ec, error_code());
      BOOST_TEST(p.done());
   }

   // Parse
   auto ec = parse_sentinel_response(nodes, 2u, resp);

   const address expected_sentinels[] = {
      {"host.one", "26380"},
      {"host.two", "26381"},
   };

   BOOST_TEST_EQ(ec, error_code());
   BOOST_TEST_EQ(resp.diagnostic, "");
   BOOST_TEST_EQ(resp.server_addr, (address{"localhost", "6380"}));
   BOOST_TEST_ALL_EQ(
      resp.sentinels.begin(),
      resp.sentinels.end(),
      std::begin(expected_sentinels),
      std::end(expected_sentinels));
}

}  // namespace

int main()
{
   test_success();

   return boost::report_errors();
}
