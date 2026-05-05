//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/co_connection.hpp>
#include <boost/redis/src/corosio.hpp>
#include <boost/redis/src/proto.hpp>

#include <boost/corosio/io_context.hpp>

int main()
{
   boost::corosio::io_context ctx;
   boost::redis::co_connection conn(ctx);
   return 0;
}
