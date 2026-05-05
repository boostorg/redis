/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/co_connection.hpp>
#include <boost/redis/src/corosio.hpp>
#include <boost/redis/src/proto.hpp>

#include <boost/capy/ex/thread_pool.hpp>

int main()
{
   boost::capy::thread_pool pool(1);
   boost::redis::co_connection conn(pool.get_executor());
   return 0;
}
