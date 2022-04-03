/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>
#include <map>

#include <boost/system/errc.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/connect.hpp>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

#include "check.hpp"

namespace net = boost::asio;
namespace resp3 = aedis::resp3;

using aedis::redis::command;
using aedis::resp3::make_serializer;
using aedis::adapter::adapt;
using node_type = aedis::resp3::node<std::string>;
using tcp = net::ip::tcp;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<tcp::socket>;

std::vector<node_type> gresp;

net::awaitable<void>
test_general(net::ip::tcp::resolver::results_type const& res)
{
   auto ex = co_await net::this_coro::executor;

   std::vector<int> list_ {1 ,2, 3, 4, 5, 6};
   std::string set_ {"aaa"};

   //----------------------------------
   std::string request;
   auto sr = make_serializer(request);
   sr.push(command::hello, 3);
   sr.push(command::flushall);
   sr.push_range(command::rpush, "a", list_);
   sr.push(command::llen, "a");
   sr.push(command::lrange, "a", 0, -1);
   sr.push(command::ltrim, "a", 2, -2);
   sr.push(command::lpop, "a");
   //sr.lpop("a", 2); // Not working?
   sr.push(command::set, "b", set_);
   sr.push(command::get, "b");
   sr.push(command::append, "b", "b");
   sr.push(command::del, "b");
   sr.push(command::subscribe, "channel");
   sr.push(command::incr, "3");

   // transaction
   for (auto i = 0; i < 3; ++i) {
      sr.push(command::multi);
      sr.push(command::ping);
      sr.push(command::lrange, "a", 0, -1);
      sr.push(command::ping);
      // TODO: It looks like we can't publish to a channel we
      // are already subscribed to from inside a transaction.
      //req.publish("some-channel", "message1");
      sr.push(command::exec);
   }

   std::map<std::string, std::string> m1 =
   { {"field1", "value1"}
   , {"field2", "value2"}};

   sr.push_range(command::hset, "d", m1);
   sr.push(command::hget, "d", "field2");
   sr.push(command::hgetall, "d");
   sr.push(command::hdel, "d", "field1", "field2"); // TODO: Test as range too.
   sr.push(command::hincrby, "e", "some-field", 10);

   sr.push(command::zadd, "f", 1, "Marcelo");
   sr.push(command::zrange, "f", 0, 1);
   sr.push(command::zrangebyscore, "f", 1, 1);
   sr.push(command::zremrangebyscore, "f", "-inf", "+inf");

   auto const v = std::vector<int>{1, 2, 3};
   sr.push_range(command::sadd, "g", v);
   sr.push(command::smembers, "g");
   sr.push(command::quit);
   //----------------------------------

   net::ip::tcp::socket socket{ex};
   co_await net::async_connect(socket, res, net::use_awaitable);
   co_await net::async_write(socket, net::buffer(request), net::use_awaitable);

   // Reads the responses.
   std::string buffer;

   // hello
   co_await resp3::async_read(socket, net::dynamic_buffer(buffer), adapt(), net::use_awaitable);

   // flushall
   co_await resp3::async_read(socket, net::dynamic_buffer(buffer), adapt(), net::use_awaitable);

   { // rpush:
      std::vector<node_type> resp;
      co_await resp3::async_read(socket, net::dynamic_buffer(buffer), adapt(resp), net::use_awaitable);
      auto const n = std::to_string(std::size(list_));
      std::vector<node_type> expected
	 { {resp3::type::number, 1UL, 0UL, n} };

     expect_eq(resp, expected, "rpush (value)");
   }

   { // llen
      std::vector<node_type> resp;
      co_await resp3::async_read(socket, net::dynamic_buffer(buffer), adapt(resp), net::use_awaitable);
      std::vector<node_type> expected
	 { {resp3::type::number, 1UL, 0UL, {"6"}} };
      expect_eq(resp, expected, "llen");
   }

   { // lrange
      std::vector<node_type> resp;
      co_await resp3::async_read(socket, net::dynamic_buffer(buffer), adapt(resp), net::use_awaitable);

      std::vector<node_type> expected
         { {resp3::type::array,       6UL, 0UL, {}}
         , {resp3::type::blob_string, 1UL, 1UL, {"1"}}
         , {resp3::type::blob_string, 1UL, 1UL, {"2"}}
         , {resp3::type::blob_string, 1UL, 1UL, {"3"}}
         , {resp3::type::blob_string, 1UL, 1UL, {"4"}}
         , {resp3::type::blob_string, 1UL, 1UL, {"5"}}
         , {resp3::type::blob_string, 1UL, 1UL, {"6"}}
         };

      expect_eq(resp, expected, "lrange ");
   }

   {  // ltrim
      std::vector<node_type> resp;
      co_await resp3::async_read(socket, net::dynamic_buffer(buffer), adapt(resp), net::use_awaitable);

      std::vector<node_type> expected
         { {resp3::type::simple_string, 1UL, 0UL, {"OK"}} };

      expect_eq(resp, expected, "ltrim");
   }

   {  // lpop
      std::vector<node_type> resp;
      co_await resp3::async_read(socket, net::dynamic_buffer(buffer), adapt(resp), net::use_awaitable);

      std::vector<node_type> expected
         { {resp3::type::blob_string, 1UL, 0UL, {"3"}} };

      expect_eq(resp, expected, "lpop");
   }

   //{  // lpop
   //   std::vector<node_type> resp;
   //   co_await resp3::async_read(socket, net::dynamic_buffer(buffer), adapt(resp), net::use_awaitable);

   //   std::vector<node_type> expected
   //      { {resp3::type::array, 2UL, 0UL, {}}
   //      , {resp3::type::array, 1UL, 1UL, {"4"}}
   //      , {resp3::type::array, 1UL, 1UL, {"5"}}
   //      };

   //   expect_eq(resp, expected, "lpop");
   //}

   //{ // lrange
   //   static int c = 0;

   //   if (c == 0) {
   //     std::vector<node_type> expected
   //         { {resp3::type::array,       6UL, 0UL, {}}
   //         , {resp3::type::blob_string, 1UL, 1UL, {"1"}}
   //         , {resp3::type::blob_string, 1UL, 1UL, {"2"}}
   //         , {resp3::type::blob_string, 1UL, 1UL, {"3"}}
   //         , {resp3::type::blob_string, 1UL, 1UL, {"4"}}
   //         , {resp3::type::blob_string, 1UL, 1UL, {"5"}}
   //         , {resp3::type::blob_string, 1UL, 1UL, {"6"}}
   //         };

   //      expect_eq(resp, expected, "lrange ");
   //   } else {
   //     std::vector<node_type> expected
   //         { {resp3::type::simple_string, 1UL, 0UL, {"QUEUED"}} };

   //      expect_eq(resp, expected, "lrange (inside transaction)");
   //   }
   //   
   //   ++c;
   //}

   //for (;;) {
   //   switch (cmd) {
   //      case command::multi:
   //      {
   //        std::vector<node_type> expected
   //         { {resp3::type::simple_string, 1UL, 0UL, {"OK"}} };

   //         expect_eq(resp, expected, "multi");
   //      } break;
   //      case command::ping:
   //      {
   //        std::vector<node_type> expected
   //         { {resp3::type::simple_string, 1UL, 0UL, {"QUEUED"}} };

   //         expect_eq(resp, expected, "ping");
   //      } break;
   //      case command::set:
   //      {
   //        std::vector<node_type> expected
   //         { {resp3::type::simple_string, 1UL, 0UL, {"OK"}} };

   //         expect_eq(resp, expected, "set");
   //      } break;
   //      case command::quit:
   //      {
   //        std::vector<node_type> expected
   //         { {resp3::type::simple_string, 1UL, 0UL, {"OK"}} };

   //         expect_eq(resp, expected, "quit");
   //      } break;
   //      case command::flushall:
   //      {
   //        std::vector<node_type> expected
   //         { {resp3::type::simple_string, 1UL, 0UL, {"OK"}} };

   //         expect_eq(resp, expected, "flushall");
   //      } break;
   //      case command::append:
   //      {
   //        std::vector<node_type> expected
   //         { {resp3::type::number, 1UL, 0UL, {"4"}} };

   //        expect_eq(resp, expected, "append");
   //      } break;
   //      case command::hset:
   //      {
   //        std::vector<node_type> expected
   //         { {resp3::type::number, 1UL, 0UL, {"2"}} };

   //        expect_eq(resp, expected, "hset");
   //      } break;
   //      case command::del:
   //      {
   //        std::vector<node_type> expected
   //         { {resp3::type::number, 1UL, 0UL, {"1"}} };

   //        expect_eq(resp, expected, "del");
   //      } break;
   //      case command::incr:
   //      {
   //        std::vector<node_type> expected
   //         { {resp3::type::number, 1UL, 0UL, {"1"}} };

   //        expect_eq(resp, expected, "incr");
   //      } break;
   //      case command::publish:
   //      {
   //        std::vector<node_type> expected
   //         { {resp3::type::number, 1UL, 0UL, {"1"}} };

   //        expect_eq(resp, expected, "publish");
   //      } break;
   //      case command::hincrby:
   //      {
   //        std::vector<node_type> expected
   //         { {resp3::type::number, 1UL, 0UL, {"10"}} };

   //        expect_eq(resp, expected, "hincrby");
   //      } break;
   //      case command::zadd:
   //      {
   //        std::vector<node_type> expected
   //         { {resp3::type::number, 1UL, 0UL, {"1"}} };

   //        expect_eq(resp, expected, "zadd");
   //      } break;
   //      case command::sadd:
   //      {
   //        std::vector<node_type> expected
   //         { {resp3::type::number, 1UL, 0UL, {"3"}} };

   //        expect_eq(resp, expected, "sadd");
   //      } break;
   //      case command::hdel:
   //      {
   //        std::vector<node_type> expected
   //         { {resp3::type::number, 1UL, 0UL, {"2"}} };

   //        expect_eq(resp, expected, "hdel");
   //      } break;
   //      case command::zremrangebyscore:
   //      {
   //        std::vector<node_type> expected
   //         { {resp3::type::number, 1UL, 0UL, {"1"}} };

   //        expect_eq(resp, expected, "zremrangebyscore");
   //      } break;
   //      case command::get:
   //      {
   //        std::vector<node_type> expected
   //            { {resp3::type::blob_string, 1UL, 0UL, test.set_} };

   //         expect_eq(resp, expected, "get");
   //      } break;
   //      case command::hget:
   //      {
   //        std::vector<node_type> expected
   //            { {resp3::type::blob_string, 1UL, 0UL, std::string{"value2"}} };

   //         expect_eq(resp, expected, "hget");
   //      } break;
   //      case command::hvals:
   //      {
   //        std::vector<node_type> expected
   //            { {resp3::type::array, 2UL, 0UL, {}}
   //            , {resp3::type::array, 1UL, 1UL, {"value1"}}
   //            , {resp3::type::array, 1UL, 1UL, {"value2"}}
   //            };

   //        expect_eq(resp, expected, "hvals");
   //      } break;
   //      case command::zrange:
   //      {
   //        std::vector<node_type> expected
   //            { {resp3::type::array,       1UL, 0UL, {}}
   //            , {resp3::type::blob_string, 1UL, 1UL, {"Marcelo"}}
   //            };

   //        expect_eq(resp, expected, "hvals");
   //      } break;
   //      case command::zrangebyscore:
   //      {
   //        std::vector<node_type> expected
   //            { {resp3::type::array,       1UL, 0UL, {}}
   //            , {resp3::type::blob_string, 1UL, 1UL, {"Marcelo"}}
   //            };

   //        expect_eq(resp, expected, "zrangebyscore");
   //      } break;
   //      case command::exec:
   //      {
   //        std::vector<node_type> expected
   //            { {resp3::type::array,         3UL, 0UL, {}}
   //            , {resp3::type::simple_string, 1UL, 1UL, {"PONG"}}
   //            , {resp3::type::array,         2UL, 1UL, {}}
   //            , {resp3::type::blob_string,   1UL, 2UL, {"4"}}
   //            , {resp3::type::blob_string,   1UL, 2UL, {"5"}}
   //            , {resp3::type::simple_string, 1UL, 1UL, {"PONG"}}
   //            };

   //         expect_eq(resp, expected, "transaction");

   //      } break;
   //      case command::hgetall:
   //      {
   //        std::vector<node_type> expected
   //         { {resp3::type::map,         2UL, 0UL, {}}
   //         , {resp3::type::blob_string, 1UL, 1UL, {"field1"}}
   //         , {resp3::type::blob_string, 1UL, 1UL, {"value1"}}
   //         , {resp3::type::blob_string, 1UL, 1UL, {"field2"}}
   //         , {resp3::type::blob_string, 1UL, 1UL, {"value2"}}
   //         };
   //         expect_eq(resp, expected, "hgetall (value)");
   //      } break;
   //      case command::smembers:
   //      {
   //        std::vector<node_type> expected
   //         { {resp3::type::set,          3UL, 0UL, {}}
   //         , {resp3::type::blob_string,  1UL, 1UL, {"1"}}
   //         , {resp3::type::blob_string,  1UL, 1UL, {"2"}}
   //         , {resp3::type::blob_string,  1UL, 1UL, {"3"}}
   //         };
   //         expect_eq(resp, expected, "smembers (value)");
   //      } break;
   //      default: { std::cout << "Error: " << resp.front().data_type << " " << cmd << std::endl; }
   //   }

   //   resp.clear();
   //}
}

//-------------------------------------------------------------------

int main()
{
   net::io_context ioc {1};
   tcp::resolver resv(ioc);
   auto const res = resv.resolve("127.0.0.1", "6379");

   co_spawn(ioc, test_general(res), net::detached);
   ioc.run();
}

