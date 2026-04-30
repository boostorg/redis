//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// The Asio and Corosio implementations can be mixed freely without ODR violations.
// Mixing all the src files is OK, too
#include <boost/redis/src/asio.hpp>
#include <boost/redis/src/corosio.hpp>
#include <boost/redis/src/proto.hpp>

// Regular includes
#include <boost/redis/co_connection.hpp>
#include <boost/redis/connection.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/capy/ex/this_coro.hpp>
#include <boost/capy/task.hpp>
#include <boost/capy/timeout.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/corosio/io_context.hpp>

using namespace boost::redis;
namespace asio = boost::asio;
namespace capy = boost::capy;
namespace corosio = boost::corosio;
using namespace std::chrono_literals;

namespace {

void test_corosio()
{
   corosio::io_context ctx;
   co_connection conn{ctx};
}

void test_asio()
{
   asio::io_context ctx;
   connection conn{ctx};
}

}  // namespace

int main()
{
   test_corosio();
   test_asio();

   return boost::report_errors();
}
