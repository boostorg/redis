//
// Copyright (c) 2025-2026 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/detail/subscription_tracker.hpp>
#include <boost/redis/request.hpp>

#include <boost/core/lightweight_test.hpp>

using namespace boost::redis;
using detail::subscription_tracker;

namespace {

// State originated by SUBSCRIBE commands, only
void test_subscribe()
{
   subscription_tracker tracker;
   request req1, req2, req_output, req_expected;

   // Add some changes to the tracker
   req1.subscribe({"channel_a", "channel_b"});
   tracker.commit_changes(req1);

   req2.subscribe({"channel_c"});
   tracker.commit_changes(req2);

   // Check that we generate the correct response
   tracker.compose_subscribe_request(req_output);
   req_expected.push("SUBSCRIBE", "channel_a", "channel_b", "channel_c");
   BOOST_TEST_EQ(req_output.payload(), req_expected.payload());
}

}  // namespace

int main()
{
   test_subscribe();

   return boost::report_errors();
}
