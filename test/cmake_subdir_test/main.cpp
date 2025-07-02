/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>
#include <boost/redis/src.hpp>

int main()
{
   boost::redis::connection conn(boost::asio::system_executor{});
   return static_cast<int>(!conn.will_reconnect());
}
