/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>
#include <map>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>
#include "test_stream.hpp"
#include "check.hpp"

namespace net = aedis::net;
using tcp = net::ip::tcp;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<tcp::socket>;
using test_tcp_socket = net::use_awaitable_t<>::as_default_on_t<aedis::test_stream<aedis::net::system_executor>>;

namespace this_coro = net::this_coro;

using namespace aedis;
using namespace aedis::resp3;
using aedis::redis::command;
using aedis::redis::make_serializer;

std::vector<node> gresp;

//-------------------------------------------------------------------

net::awaitable<void>
test_general(net::ip::tcp::resolver::results_type const& res)
{
   auto ex = co_await this_coro::executor;

   std::vector<int> list_ {1 ,2, 3, 4, 5, 6};
   std::string set_ {"aaa"};

   //----------------------------------
   std::string request;
   auto sr = make_serializer(request);
   sr.push(command::hello, 3);
   sr.push(command::flushall);
   sr.push_range(command::rpush, "a", std::cbegin(list_), std::cend(list_));
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

   sr.push_range(command::hset, "d", std::cbegin(m1), std::cend(m1));
   sr.push(command::hget, "d", "field2");
   sr.push(command::hgetall, "d");
   sr.push(command::hdel, "d", "field1", "field2"); // TODO: Test as range too.
   sr.push(command::hincrby, "e", "some-field", 10);

   sr.push(command::zadd, "f", 1, "Marcelo");
   sr.push(command::zrange, "f", 0, 1);
   sr.push(command::zrangebyscore, "f", 1, 1);
   sr.push(command::zremrangebyscore, "f", "-inf", "+inf");

   auto const v = std::vector<int>{1, 2, 3};
   sr.push_range(command::sadd, "g", std::cbegin(v), std::cend(v));
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
      std::vector<node> resp;
      co_await resp3::async_read(socket, net::dynamic_buffer(buffer), adapt(resp), net::use_awaitable);
      auto const n = std::to_string(std::size(list_));
      std::vector<node> expected
	 { {resp3::type::number, 1UL, 0UL, n} };

     check_equal(resp, expected, "rpush (value)");
   }

   { // llen
      std::vector<node> resp;
      co_await resp3::async_read(socket, net::dynamic_buffer(buffer), adapt(resp), net::use_awaitable);
      std::vector<node> expected
	 { {resp3::type::number, 1UL, 0UL, {"6"}} };
      check_equal(resp, expected, "llen");
   }

   { // lrange
      std::vector<node> resp;
      co_await resp3::async_read(socket, net::dynamic_buffer(buffer), adapt(resp), net::use_awaitable);

      std::vector<node> expected
         { {resp3::type::array,       6UL, 0UL, {}}
         , {resp3::type::blob_string, 1UL, 1UL, {"1"}}
         , {resp3::type::blob_string, 1UL, 1UL, {"2"}}
         , {resp3::type::blob_string, 1UL, 1UL, {"3"}}
         , {resp3::type::blob_string, 1UL, 1UL, {"4"}}
         , {resp3::type::blob_string, 1UL, 1UL, {"5"}}
         , {resp3::type::blob_string, 1UL, 1UL, {"6"}}
         };

      check_equal(resp, expected, "lrange ");
   }

   {  // ltrim
      std::vector<node> resp;
      co_await resp3::async_read(socket, net::dynamic_buffer(buffer), adapt(resp), net::use_awaitable);

      std::vector<node> expected
         { {resp3::type::simple_string, 1UL, 0UL, {"OK"}} };

      check_equal(resp, expected, "ltrim");
   }

   {  // lpop
      std::vector<node> resp;
      co_await resp3::async_read(socket, net::dynamic_buffer(buffer), adapt(resp), net::use_awaitable);

      std::vector<node> expected
         { {resp3::type::blob_string, 1UL, 0UL, {"3"}} };

      check_equal(resp, expected, "lpop");
   }

   //{  // lpop
   //   std::vector<node> resp;
   //   co_await resp3::async_read(socket, net::dynamic_buffer(buffer), adapt(resp), net::use_awaitable);

   //   std::vector<node> expected
   //      { {resp3::type::array, 2UL, 0UL, {}}
   //      , {resp3::type::array, 1UL, 1UL, {"4"}}
   //      , {resp3::type::array, 1UL, 1UL, {"5"}}
   //      };

   //   check_equal(resp, expected, "lpop");
   //}

   //{ // lrange
   //   static int c = 0;

   //   if (c == 0) {
   //     std::vector<node> expected
   //         { {resp3::type::array,       6UL, 0UL, {}}
   //         , {resp3::type::blob_string, 1UL, 1UL, {"1"}}
   //         , {resp3::type::blob_string, 1UL, 1UL, {"2"}}
   //         , {resp3::type::blob_string, 1UL, 1UL, {"3"}}
   //         , {resp3::type::blob_string, 1UL, 1UL, {"4"}}
   //         , {resp3::type::blob_string, 1UL, 1UL, {"5"}}
   //         , {resp3::type::blob_string, 1UL, 1UL, {"6"}}
   //         };

   //      check_equal(resp, expected, "lrange ");
   //   } else {
   //     std::vector<node> expected
   //         { {resp3::type::simple_string, 1UL, 0UL, {"QUEUED"}} };

   //      check_equal(resp, expected, "lrange (inside transaction)");
   //   }
   //   
   //   ++c;
   //}

   //for (;;) {
   //   switch (cmd) {
   //      case command::multi:
   //      {
   //        std::vector<node> expected
   //         { {resp3::type::simple_string, 1UL, 0UL, {"OK"}} };

   //         check_equal(resp, expected, "multi");
   //      } break;
   //      case command::ping:
   //      {
   //        std::vector<node> expected
   //         { {resp3::type::simple_string, 1UL, 0UL, {"QUEUED"}} };

   //         check_equal(resp, expected, "ping");
   //      } break;
   //      case command::set:
   //      {
   //        std::vector<node> expected
   //         { {resp3::type::simple_string, 1UL, 0UL, {"OK"}} };

   //         check_equal(resp, expected, "set");
   //      } break;
   //      case command::quit:
   //      {
   //        std::vector<node> expected
   //         { {resp3::type::simple_string, 1UL, 0UL, {"OK"}} };

   //         check_equal(resp, expected, "quit");
   //      } break;
   //      case command::flushall:
   //      {
   //        std::vector<node> expected
   //         { {resp3::type::simple_string, 1UL, 0UL, {"OK"}} };

   //         check_equal(resp, expected, "flushall");
   //      } break;
   //      case command::append:
   //      {
   //        std::vector<node> expected
   //         { {resp3::type::number, 1UL, 0UL, {"4"}} };

   //        check_equal(resp, expected, "append");
   //      } break;
   //      case command::hset:
   //      {
   //        std::vector<node> expected
   //         { {resp3::type::number, 1UL, 0UL, {"2"}} };

   //        check_equal(resp, expected, "hset");
   //      } break;
   //      case command::del:
   //      {
   //        std::vector<node> expected
   //         { {resp3::type::number, 1UL, 0UL, {"1"}} };

   //        check_equal(resp, expected, "del");
   //      } break;
   //      case command::incr:
   //      {
   //        std::vector<node> expected
   //         { {resp3::type::number, 1UL, 0UL, {"1"}} };

   //        check_equal(resp, expected, "incr");
   //      } break;
   //      case command::publish:
   //      {
   //        std::vector<node> expected
   //         { {resp3::type::number, 1UL, 0UL, {"1"}} };

   //        check_equal(resp, expected, "publish");
   //      } break;
   //      case command::hincrby:
   //      {
   //        std::vector<node> expected
   //         { {resp3::type::number, 1UL, 0UL, {"10"}} };

   //        check_equal(resp, expected, "hincrby");
   //      } break;
   //      case command::zadd:
   //      {
   //        std::vector<node> expected
   //         { {resp3::type::number, 1UL, 0UL, {"1"}} };

   //        check_equal(resp, expected, "zadd");
   //      } break;
   //      case command::sadd:
   //      {
   //        std::vector<node> expected
   //         { {resp3::type::number, 1UL, 0UL, {"3"}} };

   //        check_equal(resp, expected, "sadd");
   //      } break;
   //      case command::hdel:
   //      {
   //        std::vector<node> expected
   //         { {resp3::type::number, 1UL, 0UL, {"2"}} };

   //        check_equal(resp, expected, "hdel");
   //      } break;
   //      case command::zremrangebyscore:
   //      {
   //        std::vector<node> expected
   //         { {resp3::type::number, 1UL, 0UL, {"1"}} };

   //        check_equal(resp, expected, "zremrangebyscore");
   //      } break;
   //      case command::get:
   //      {
   //        std::vector<node> expected
   //            { {resp3::type::blob_string, 1UL, 0UL, test.set_} };

   //         check_equal(resp, expected, "get");
   //      } break;
   //      case command::hget:
   //      {
   //        std::vector<node> expected
   //            { {resp3::type::blob_string, 1UL, 0UL, std::string{"value2"}} };

   //         check_equal(resp, expected, "hget");
   //      } break;
   //      case command::hvals:
   //      {
   //        std::vector<node> expected
   //            { {resp3::type::array, 2UL, 0UL, {}}
   //            , {resp3::type::array, 1UL, 1UL, {"value1"}}
   //            , {resp3::type::array, 1UL, 1UL, {"value2"}}
   //            };

   //        check_equal(resp, expected, "hvals");
   //      } break;
   //      case command::zrange:
   //      {
   //        std::vector<node> expected
   //            { {resp3::type::array,       1UL, 0UL, {}}
   //            , {resp3::type::blob_string, 1UL, 1UL, {"Marcelo"}}
   //            };

   //        check_equal(resp, expected, "hvals");
   //      } break;
   //      case command::zrangebyscore:
   //      {
   //        std::vector<node> expected
   //            { {resp3::type::array,       1UL, 0UL, {}}
   //            , {resp3::type::blob_string, 1UL, 1UL, {"Marcelo"}}
   //            };

   //        check_equal(resp, expected, "zrangebyscore");
   //      } break;
   //      case command::exec:
   //      {
   //        std::vector<node> expected
   //            { {resp3::type::array,         3UL, 0UL, {}}
   //            , {resp3::type::simple_string, 1UL, 1UL, {"PONG"}}
   //            , {resp3::type::array,         2UL, 1UL, {}}
   //            , {resp3::type::blob_string,   1UL, 2UL, {"4"}}
   //            , {resp3::type::blob_string,   1UL, 2UL, {"5"}}
   //            , {resp3::type::simple_string, 1UL, 1UL, {"PONG"}}
   //            };

   //         check_equal(resp, expected, "transaction");

   //      } break;
   //      case command::hgetall:
   //      {
   //        std::vector<node> expected
   //         { {resp3::type::map,         2UL, 0UL, {}}
   //         , {resp3::type::blob_string, 1UL, 1UL, {"field1"}}
   //         , {resp3::type::blob_string, 1UL, 1UL, {"value1"}}
   //         , {resp3::type::blob_string, 1UL, 1UL, {"field2"}}
   //         , {resp3::type::blob_string, 1UL, 1UL, {"value2"}}
   //         };
   //         check_equal(resp, expected, "hgetall (value)");
   //      } break;
   //      case command::smembers:
   //      {
   //        std::vector<node> expected
   //         { {resp3::type::set,          3UL, 0UL, {}}
   //         , {resp3::type::blob_string,  1UL, 1UL, {"1"}}
   //         , {resp3::type::blob_string,  1UL, 1UL, {"2"}}
   //         , {resp3::type::blob_string,  1UL, 1UL, {"3"}}
   //         };
   //         check_equal(resp, expected, "smembers (value)");
   //      } break;
   //      default: { std::cout << "Error: " << resp.front().data_type << " " << cmd << std::endl; }
   //   }

   //   resp.clear();
   //}
}

//-------------------------------------------------------------------

//net::awaitable<void>
//test_list(net::ip::tcp::resolver::results_type const& results)
//{
//   std::vector<int> list {1 ,2, 3, 4, 5, 6};
//
//   resp3::serializer p;
//   p.push(command::hello, 3);
//   p.push(command::flushall);
//   p.push_range(command::rpush, "a", std::cbegin(list), std::cend(list));
//   p.push(command::lrange, "a", 0, -1);
//   p.push(command::lrange, "a", 2, -2);
//   p.push(command::ltrim, "a", 2, -2);
//   p.push(command::lpop, "a");
//   p.push(command::quit);
//
//   auto ex = co_await this_coro::executor;
//   tcp_socket socket {ex};
//   co_await async_connect(socket, results);
//   co_await async_write(socket, net::buffer(p.payload));
//   std::string buf;
//
//   {  // hello
//      gresp.clear();
//      co_await async_read(socket, buf, gresp);
//   }
//
//   {  // flushall
//      gresp.clear();
//      co_await async_read(socket, buf, gresp);
//      std::vector<node> expected
//        { {resp3::type::simple_string, 1UL, 0UL, {"OK"}} };
//      check_equal(gresp, expected, "flushall");
//   }
//
//   {  // rpush
//      gresp.clear();
//      std::vector<node> expected
//        { {resp3::type::number, 1UL, 0UL, {"6"}} };
//      co_await async_read(socket, buf, gresp);
//      check_equal(gresp, expected, "rpush");
//   }
//
//   {  // lrange
//      resp3::flat_array_int_type buffer;
//      resp3::detail::basic_flat_array_adapter<int> res{&buffer};
//      co_await async_read(socket, buf, res);
//      check_equal(buffer, list, "lrange-1");
//   }
//
//   {  // lrange
//      resp3::flat_array_int_type buffer;
//      resp3::detail::basic_flat_array_adapter<int> res{&buffer};
//      co_await async_read(socket, buf, res);
//      check_equal(buffer, std::vector<int>{3, 4, 5}, "lrange-2");
//   }
//
//   {  // ltrim
//      gresp.clear();
//      std::vector<node> expected
//	 { {resp3::type::simple_string, 1UL, 0UL, {"OK"}} };
//
//      co_await async_read(socket, buf, gresp);
//      check_equal(gresp, expected, "ltrim");
//   }
//
//   {  // lpop. Why a blob string instead of a number?
//      gresp.clear();
//      std::vector<node> expected
//	 { {resp3::type::blob_string, 1UL, 0UL, {"3"}} };
//
//      co_await async_read(socket, buf, gresp);
//      check_equal(gresp, expected, "lpop");
//   }
//
//   {  // quit
//      gresp.clear();
//      co_await async_read(socket, buf, gresp);
//      std::vector<node> expected
//      { {resp3::type::simple_string, 1UL, 0UL, {"OK"}} };
//      check_equal(gresp, expected, "ltrim");
//   }
//}

std::string test_bulk1(10000, 'a');

net::awaitable<void>
test_set(net::ip::tcp::resolver::results_type const& results)
{
   using namespace aedis;

   // Tests whether the parser can handle payloads that contain the separator.
   test_bulk1[30] = '\r';
   test_bulk1[31] = '\n';

   std::string test_bulk2 = "aaaaa";

   std::string request;
   auto sr = make_serializer(request);
   sr.push(command::hello, 3);
   sr.push(command::flushall);
   sr.push(command::set, "s", test_bulk1);
   sr.push(command::get, "s");
   sr.push(command::set, "s", test_bulk2);
   sr.push(command::get, "s");
   sr.push(command::set, "s", "");
   sr.push(command::get, "s");
   sr.push(command::quit);

   auto ex = co_await this_coro::executor;
   tcp_socket socket {ex};
   co_await async_connect(socket, results);

   co_await net::async_write(socket, net::buffer(request));

   std::string buf;
   {  // hello, flushall
      gresp.clear();
      co_await resp3::async_read(socket, net::dynamic_buffer(buf), adapt(gresp));
      co_await resp3::async_read(socket, net::dynamic_buffer(buf), adapt(gresp));
   }

   {  // set
      gresp.clear();
      co_await resp3::async_read(socket, net::dynamic_buffer(buf), adapt(gresp));
      std::vector<node> expected
      { {resp3::type::simple_string, 1UL, 0UL, {"OK"}} };
      check_equal(gresp, expected, "set1");
   }

   {  // get
      gresp.clear();
      std::vector<node> expected
	 { {resp3::type::blob_string, 1UL, 0UL, test_bulk1} };

      co_await resp3::async_read(socket, net::dynamic_buffer(buf), adapt(gresp));
      check_equal(gresp, expected, "get1");
   }

   {  // set
      gresp.clear();
      co_await resp3::async_read(socket, net::dynamic_buffer(buf), adapt(gresp));
      std::vector<node> expected
      { {resp3::type::simple_string, 1UL, 0UL, {"OK"}} };
      check_equal(gresp, expected, "ltrim");
   }

   {  // get
      gresp.clear();
      std::vector<node> expected
	 { {resp3::type::blob_string, 1UL, 0UL, test_bulk2} };
      co_await resp3::async_read(socket, net::dynamic_buffer(buf), adapt(gresp));
      check_equal(gresp, expected, "get2");
   }

   {  // set
      gresp.clear();
      co_await resp3::async_read(socket, net::dynamic_buffer(buf), adapt(gresp));
      std::vector<node> expected
      { {resp3::type::simple_string, 1UL, 0UL, {"OK"}} };
      check_equal(gresp, expected, "set3");
   }

   {  // get
      gresp.clear();
      std::vector<node> expected
	 { {resp3::type::blob_string, 1UL, 0UL, {}} };

      co_await resp3::async_read(socket, net::dynamic_buffer(buf), adapt(gresp));
      check_equal(gresp,  expected, "get3");
   }

   {  // quit
      gresp.clear();
      co_await resp3::async_read(socket, net::dynamic_buffer(buf), adapt(gresp));
      std::vector<node> expected
      { {resp3::type::simple_string, 1UL, 0UL, {"OK"}} };
      check_equal(gresp, expected, "quit");
   }
}

int main()
{
   net::io_context ioc {1};
   tcp::resolver resv(ioc);
   auto const res = resv.resolve("127.0.0.1", "6379");

   //co_spawn(ioc, test_list(res), net::detached);
   co_spawn(ioc, test_set(res), net::detached);
   co_spawn(ioc, test_general(res), net::detached);
   ioc.run();
}

