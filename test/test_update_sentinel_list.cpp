//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/config.hpp>
#include <boost/redis/impl/update_sentinel_list.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/system/error_code.hpp>

#include <vector>

using namespace boost::redis;
using detail::update_sentinel_list;
using boost::system::error_code;

// Operators
namespace boost::redis {

std::ostream& operator<<(std::ostream& os, const address& addr)
{
   return os << "address{ .host=" << addr.host << ", .port=" << addr.port << " }";
}

}  // namespace boost::redis

namespace {

void test_single_sentinel()
{
   // Setup
   const std::vector<address> initial_sentinels{
      {"host1", "1000"}
   };
   std::vector<address> sentinels{initial_sentinels};
   update_sentinel_list(sentinels, 0u, {}, initial_sentinels);
   BOOST_TEST_ALL_EQ(
      sentinels.begin(),
      sentinels.end(),
      initial_sentinels.begin(),
      initial_sentinels.end());
}

}  // namespace

int main()
{
   test_single_sentinel();

   return boost::report_errors();
}
