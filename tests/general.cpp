/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>
#include <map>

#include <aedis/aedis.hpp>

#include "test_stream.hpp"

#include "basic_flat_array_adapter.hpp"

// TODO: Use Beast test_stream and instantiate the test socket only
// once.

namespace net = aedis::net;
using tcp = net::ip::tcp;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<tcp::socket>;
using test_tcp_socket = net::use_awaitable_t<>::as_default_on_t<aedis::test_stream<aedis::net::system_executor>>;

namespace this_coro = net::this_coro;

namespace aedis {
namespace resp3 {
using flat_array_adapter = detail::basic_flat_array_adapter<std::string>;
using flat_array_int_adapter = detail::basic_flat_array_adapter<int>;
using flat_array_int_type = detail::basic_flat_array<int>;

} // resp3
} // aedis

using namespace aedis;
using namespace aedis::resp3;

resp3::response gresp;

template <class T>
void check_equal(T const& a, T const& b, std::string const& msg = "")
{
   if (a == b)
     std::cout << "Success: " << msg << std::endl;
   else
     std::cout << "Error: " << msg << std::endl;
}

//-------------------------------------------------------------------

struct test_general_fill {
   std::vector<int> list_ {1 ,2, 3, 4, 5, 6};
   std::string set_ {"aaa"};

   void operator()(resp3::request<command>& p) const
   {
      p.push(command::flushall);
      p.push_range(command::rpush, "a", std::cbegin(list_), std::cend(list_));
      p.push(command::llen, "a");
      p.push(command::lrange, "a", 0, -1);
      p.push(command::ltrim, "a", 2, -2);
      p.push(command::lpop, "a");
      //p.lpop("a", 2); // Not working?
      p.push(command::set, "b", set_);
      p.push(command::get, "b");
      p.push(command::append, "b", "b");
      p.push(command::del, "b");
      p.push(command::subscribe, "channel");
      p.push(command::publish, "channel", "message");
      p.push(command::incr, "3");

      //----------------------------------
      // transaction
      for (auto i = 0; i < 3; ++i) {
	 p.push(command::multi);
	 p.push(command::ping);
	 p.push(command::lrange, "a", 0, -1);
	 p.push(command::ping);
	 // TODO: It looks like we can't publish to a channel we
	 // are already subscribed to from inside a transaction.
	 //req.publish("some-channel", "message1");
	 p.push(command::exec);
      }
      //----------------------------------

      std::map<std::string, std::string> m1 =
      { {"field1", "value1"}
      , {"field2", "value2"}};

      p.push_range(command::hset, "d", std::cbegin(m1), std::cend(m1));
      p.push(command::hget, "d", "field2");
      p.push(command::hgetall, "d");
      p.push(command::hdel, "d", "field1", "field2"); // TODO: Test as range too.
      p.push(command::hincrby, "e", "some-field", 10);

      p.push(command::zadd, "f", 1, "Marcelo");
      p.push(command::zrange, "f", 0, 1);
      p.push(command::zrangebyscore, "f", 1, 1);
      p.push(command::zremrangebyscore, "f", "-inf", "+inf");

      auto const v = std::vector<int>{1, 2, 3};
      p.push_range(command::sadd, "g", std::cbegin(v), std::cend(v));
      p.push(command::smembers, "g");

      p.push(command::quit);
   }
};

net::awaitable<void>
test_general(net::ip::tcp::resolver::results_type const& res)
{
   std::queue<resp3::request<command>> requests;
   requests.push({});
   requests.back().push(command::hello, 3);
   test_general_fill filler;
   filler(requests.back());

   auto ex = co_await this_coro::executor;
   net::ip::tcp::socket socket{ex};
   co_await net::async_connect(socket, res, net::use_awaitable);

   co_await async_write(socket, requests.back(), net::use_awaitable);

   std::string buffer;
   resp3::response resp;
   int push_counter = 0;
   for (;;) {
      resp.clear();
      co_await resp3::async_read(socket, buffer, resp, net::use_awaitable);

      if (resp.get_type() == resp3::type::push) {
	 switch (push_counter) {
	    case 0:
	    {
	       resp3::response::storage_type expected
	       { {3UL, 0UL, resp3::type::push, {}}
	       , {1UL, 1UL, resp3::type::blob_string, {"subscribe"}}
	       , {1UL, 1UL, resp3::type::blob_string, {"channel"}}
	       , {1UL, 1UL, resp3::type::number, {"1"}}
	       };

	       check_equal(resp.raw(), expected, "push (value1)"); break;
	    } break;
	    case 1:
	    {
	       resp3::response::storage_type expected
	       { {3UL, 0UL, resp3::type::push, {}}
	       , {1UL, 1UL, resp3::type::blob_string, {"message"}}
	       , {1UL, 1UL, resp3::type::blob_string, {"channel"}}
	       , {1UL, 1UL, resp3::type::blob_string, {"message"}}
	       };

	       check_equal(resp.raw(), expected, "push (value2)"); break;
	    } break;
	    default: std::cout << "ERROR: unexpected push event." << std::endl;
	 }
	 ++push_counter;
         continue;
      }

      auto const cmd = requests.front().commands.front();
      requests.front().commands.pop();

      switch (cmd) {
	 case command::hello:
	 {
	 } break;
	 case command::multi:
	 {
	    resp3::response::storage_type expected
	    { {1UL, 0UL, resp3::type::simple_string, {"OK"}} };

	    check_equal(resp.raw(), expected, "multi");
	 } break;
	 case command::ping:
	 {
	    resp3::response::storage_type expected
	    { {1UL, 0UL, resp3::type::simple_string, {"QUEUED"}} };

	    check_equal(resp.raw(), expected, "ping");
	 } break;
	 case command::set:
	 {
	    resp3::response::storage_type expected
	    { {1UL, 0UL, resp3::type::simple_string, {"OK"}} };

	    check_equal(resp.raw(), expected, "set");
	 } break;
	 case command::quit:
	 {
	    resp3::response::storage_type expected
	    { {1UL, 0UL, resp3::type::simple_string, {"OK"}} };

	    check_equal(resp.raw(), expected, "quit");
	 } break;
	 case command::flushall:
	 {
	    resp3::response::storage_type expected
	    { {1UL, 0UL, resp3::type::simple_string, {"OK"}} };

	    check_equal(resp.raw(), expected, "flushall");
	 } break;
	 case command::ltrim:
	 {
	    resp3::response::storage_type expected
	    { {1UL, 0UL, resp3::type::simple_string, {"OK"}} };

	    check_equal(resp.raw(), expected, "ltrim");
	 } break;
	 case command::append:
         {
	    resp3::response::storage_type expected
	    { {1UL, 0UL, resp3::type::number, {"4"}} };

           check_equal(resp.raw(), expected, "append");
         } break;
	 case command::hset:
         {
	    resp3::response::storage_type expected
	    { {1UL, 0UL, resp3::type::number, {"2"}} };

           check_equal(resp.raw(), expected, "hset");
         } break;
	 case command::rpush:
         {
           auto const n = std::to_string(std::size(filler.list_));
	    resp3::response::storage_type expected
	    { {1UL, 0UL, resp3::type::number, n} };

           check_equal(resp.raw(), expected, "rpush (value)");
         } break;
	 case command::del:
         {
	    resp3::response::storage_type expected
	    { {1UL, 0UL, resp3::type::number, {"1"}} };

           check_equal(resp.raw(), expected, "del");
         } break;
	 case command::llen:
         {
	    resp3::response::storage_type expected
	    { {1UL, 0UL, resp3::type::number, {"6"}} };

           check_equal(resp.raw(), expected, "llen");
         } break;
	 case command::incr:
         {
	    resp3::response::storage_type expected
	    { {1UL, 0UL, resp3::type::number, {"1"}} };

           check_equal(resp.raw(), expected, "incr");
         } break;
	 case command::publish:
         {
	    resp3::response::storage_type expected
	    { {1UL, 0UL, resp3::type::number, {"1"}} };

           check_equal(resp.raw(), expected, "publish");
         } break;
	 case command::hincrby:
         {
	    resp3::response::storage_type expected
	    { {1UL, 0UL, resp3::type::number, {"10"}} };

           check_equal(resp.raw(), expected, "hincrby");
         } break;
	 case command::zadd:
         {
	    resp3::response::storage_type expected
	    { {1UL, 0UL, resp3::type::number, {"1"}} };

           check_equal(resp.raw(), expected, "zadd");
         } break;
	 case command::sadd:
         {
	    resp3::response::storage_type expected
	    { {1UL, 0UL, resp3::type::number, {"3"}} };

           check_equal(resp.raw(), expected, "sadd");
         } break;
	 case command::hdel:
         {
	    resp3::response::storage_type expected
	    { {1UL, 0UL, resp3::type::number, {"2"}} };

           check_equal(resp.raw(), expected, "hdel");
         } break;
	 case command::zremrangebyscore:
         {
	    resp3::response::storage_type expected
	    { {1UL, 0UL, resp3::type::number, {"1"}} };

           check_equal(resp.raw(), expected, "zremrangebyscore");
         } break;
	 case command::get:
	 {
	    resp3::response::storage_type expected
	       { {1UL, 0UL, resp3::type::blob_string, filler.set_} };

	    check_equal(resp.raw(), expected, "get");
	 } break;
	 case command::hget:
	 {
	    resp3::response::storage_type expected
	       { {1UL, 0UL, resp3::type::blob_string, std::string{"value2"}} };

	    check_equal(resp.raw(), expected, "hget");
	 } break;
	 case command::lrange:
         {
            static int c = 0;

            if (c == 0) {
               resp3::response::storage_type expected
                  { {6UL, 0UL, resp3::type::array, {}}
                  , {1UL, 1UL, resp3::type::blob_string, {"1"}}
                  , {1UL, 1UL, resp3::type::blob_string, {"2"}}
                  , {1UL, 1UL, resp3::type::blob_string, {"3"}}
                  , {1UL, 1UL, resp3::type::blob_string, {"4"}}
                  , {1UL, 1UL, resp3::type::blob_string, {"5"}}
                  , {1UL, 1UL, resp3::type::blob_string, {"6"}}
                  };

               check_equal(resp.raw(), expected, "lrange ");
            } else {
               resp3::response::storage_type expected
                  { {1UL, 0UL, resp3::type::simple_string, {"QUEUED"}} };

               check_equal(resp.raw(), expected, "lrange (inside transaction)");
            }
            
            ++c;
         } break;
	 case command::hvals:
         {
	    resp3::response::storage_type expected
	       { {2UL, 0UL, resp3::type::array, {}}
	       , {1UL, 1UL, resp3::type::array, {"value1"}}
	       , {1UL, 1UL, resp3::type::array, {"value2"}}
               };

           check_equal(resp.raw(), expected, "hvals");
         } break;
	 case command::zrange:
         {
	    resp3::response::storage_type expected
	       { {1UL, 0UL, resp3::type::array, {}}
	       , {1UL, 1UL, resp3::type::blob_string, {"Marcelo"}}
               };

           check_equal(resp.raw(), expected, "hvals");
         } break;
	 case command::zrangebyscore:
         {
	    resp3::response::storage_type expected
	       { {1UL, 0UL, resp3::type::array, {}}
	       , {1UL, 1UL, resp3::type::blob_string, {"Marcelo"}}
               };

           check_equal(resp.raw(), expected, "zrangebyscore");
         } break;
	 case command::lpop:
	 {
	    switch (resp.get_type())
	    {
	       case resp3::type::blob_string:
	       {
	          resp3::response::storage_type expected
		     { {1UL, 0UL, resp3::type::blob_string, {"3"}} };

	          check_equal(resp.raw(), expected, "lpop");
	       } break;
	       case resp3::type::array:
               {
                 resp3::response::storage_type expected
                    { {2UL, 0UL, resp3::type::array, {}}
                    , {1UL, 1UL, resp3::type::array, {"4"}}
                    , {1UL, 1UL, resp3::type::array, {"5"}}
                    };

                 check_equal(resp.raw(), {"4", "5"}, "lpop");
               } break;
	       default:
               {
                 std::cout << "Error." << std::endl;
               }
	    }
	 } break;
	 case command::exec:
	 {
            resp3::response::storage_type expected
               { {3UL, 0UL, resp3::type::array, {}}
               , {1UL, 1UL, resp3::type::simple_string, {"PONG"}}
               , {2UL, 1UL, resp3::type::array, {}}
               , {1UL, 2UL, resp3::type::blob_string, {"4"}}
               , {1UL, 2UL, resp3::type::blob_string, {"5"}}
               , {1UL, 1UL, resp3::type::simple_string, {"PONG"}}
               };

	    check_equal(resp.raw(), expected, "transaction");

	 } break;
	 case command::hgetall:
	 {
	    resp3::response::storage_type expected
	    { {2UL, 0UL, resp3::type::map,    {}}
	    , {1UL, 1UL, resp3::type::blob_string, {"field1"}}
	    , {1UL, 1UL, resp3::type::blob_string, {"value1"}}
	    , {1UL, 1UL, resp3::type::blob_string, {"field2"}}
	    , {1UL, 1UL, resp3::type::blob_string, {"value2"}}
	    };
	    check_equal(resp.raw(), expected, "hgetall (value)");
	 } break;
	 case command::smembers:
	 {
	    resp3::response::storage_type expected
	    { { 3UL, 0UL, resp3::type::set,    {}}
	    , { 1UL, 1UL, resp3::type::blob_string, {"1"}}
	    , { 1UL, 1UL, resp3::type::blob_string, {"2"}}
	    , { 1UL, 1UL, resp3::type::blob_string, {"3"}}
	    };
	    check_equal(resp.raw(), expected, "smembers (value)");
	 } break;
	 default: { std::cout << "Error: " << resp.get_type() << " " << cmd << std::endl; }
      }

      resp.raw().clear();
   }
}

//-------------------------------------------------------------------

//net::awaitable<void>
//test_list(net::ip::tcp::resolver::results_type const& results)
//{
//   std::vector<int> list {1 ,2, 3, 4, 5, 6};
//
//   resp3::request p;
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
//      resp3::response::storage_type expected
//        { {1UL, 0UL, resp3::type::simple_string, {"OK"}} };
//      check_equal(gresp.raw(), expected, "flushall");
//   }
//
//   {  // rpush
//      gresp.clear();
//      resp3::response::storage_type expected
//        { {1UL, 0UL, resp3::type::number, {"6"}} };
//      co_await async_read(socket, buf, gresp);
//      check_equal(gresp.raw(), expected, "rpush");
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
//      resp3::response::storage_type expected
//	 { {1UL, 0UL, resp3::type::simple_string, {"OK"}} };
//
//      co_await async_read(socket, buf, gresp);
//      check_equal(gresp.raw(), expected, "ltrim");
//   }
//
//   {  // lpop. Why a blob string instead of a number?
//      gresp.clear();
//      resp3::response::storage_type expected
//	 { {1UL, 0UL, resp3::type::blob_string, {"3"}} };
//
//      co_await async_read(socket, buf, gresp);
//      check_equal(gresp.raw(), expected, "lpop");
//   }
//
//   {  // quit
//      gresp.clear();
//      co_await async_read(socket, buf, gresp);
//      resp3::response::storage_type expected
//      { {1UL, 0UL, resp3::type::simple_string, {"OK"}} };
//      check_equal(gresp.raw(), expected, "ltrim");
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

   resp3::request<command> p;
   p.push(command::hello, 3);
   p.push(command::flushall);
   p.push(command::set, "s", test_bulk1);
   p.push(command::get, "s");
   p.push(command::set, "s", test_bulk2);
   p.push(command::get, "s");
   p.push(command::set, "s", "");
   p.push(command::get, "s");
   p.push(command::quit);

   auto ex = co_await this_coro::executor;
   tcp_socket socket {ex};
   co_await async_connect(socket, results);

   co_await async_write(socket, p);

   std::string buf;
   {  // hello, flushall
      gresp.clear();
      co_await async_read(socket, buf, gresp);
      co_await async_read(socket, buf, gresp);
   }

   {  // set
      gresp.clear();
      co_await async_read(socket, buf, gresp);
      resp3::response::storage_type expected
      { {1UL, 0UL, resp3::type::simple_string, {"OK"}} };
      check_equal(gresp.raw(), expected, "set1");
   }

   {  // get
      gresp.clear();
      resp3::response::storage_type expected
	 { {1UL, 0UL, resp3::type::blob_string, test_bulk1} };

      co_await async_read(socket, buf, gresp);
      check_equal(gresp.raw(), expected, "get1");
   }

   {  // set
      gresp.clear();
      co_await async_read(socket, buf, gresp);
      resp3::response::storage_type expected
      { {1UL, 0UL, resp3::type::simple_string, {"OK"}} };
      check_equal(gresp.raw(), expected, "ltrim");
   }

   {  // get
      gresp.clear();
      resp3::response::storage_type expected
	 { {1UL, 0UL, resp3::type::blob_string, test_bulk2} };
      co_await async_read(socket, buf, gresp);
      check_equal(gresp.raw(), expected, "get2");
   }

   {  // set
      gresp.clear();
      co_await async_read(socket, buf, gresp);
      resp3::response::storage_type expected
      { {1UL, 0UL, resp3::type::simple_string, {"OK"}} };
      check_equal(gresp.raw(), expected, "set3");
   }

   {  // get
      gresp.clear();
      resp3::response::storage_type expected
	 { {1UL, 0UL, resp3::type::blob_string, {}} };

      co_await async_read(socket, buf, gresp);
      check_equal(gresp.raw(),  expected, "get3");
   }

   {  // quit
      gresp.clear();
      co_await async_read(socket, buf, gresp);
      resp3::response::storage_type expected
      { {1UL, 0UL, resp3::type::simple_string, {"OK"}} };
      check_equal(gresp.raw(), expected, "quit");
   }
}

net::awaitable<void> test_simple_string()
{
   using namespace aedis;
   {  // Small string
      std::string buf;
      std::string cmd {"+OK\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      co_await async_read(ts, buf, gresp);
      resp3::response::storage_type expected
      { {1UL, 0UL, resp3::type::simple_string, {"OK"}} };
      check_equal(gresp.raw(), expected, "simple_string");
   }

   {  // empty
      std::string buf;
      std::string cmd {"+\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      co_await async_read(ts, buf, gresp);
      resp3::response::storage_type expected
      { {1UL, 0UL, resp3::type::simple_string, {}} };
      check_equal(gresp.raw(), expected, "simple_string (empty)");
      //check_equal(res.attribute.value, {}, "simple_string (empty attribute)");
   }

   //{  // Large String (Failing because of my test stream)
   //   std::string buffer;
   //   std::string str(10000, 'a');
   //   std::string cmd;
   //   cmd += '+';
   //   cmd += str;
   //   cmd += "\r\n";
   //   test_tcp_socket ts {cmd};
   //   resp3::detail::simple_string_adapter res;
   //   co_await async_read(ts, buffer, res);
   //   check_equal(res.result, str, "simple_string (large)");
   //   //check_equal(res.attribute.value, {}, "simple_string (empty attribute)");
   //}
}

net::awaitable<void> test_number()
{
   using namespace aedis;
   std::string buf;
   {  // int
      std::string cmd {":-3\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      resp3::response::storage_type expected
        { {1UL, 0UL, resp3::type::number, {"-3"}} };
      co_await async_read(ts, buf, gresp);
      check_equal(gresp.raw(), expected, "number (int)");
   }

   {  // unsigned
      std::string cmd {":3\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      resp3::response::storage_type expected
        { {1UL, 0UL, resp3::type::number, {"3"}} };
      co_await async_read(ts, buf, gresp);
      check_equal(gresp.raw(), expected, "number (unsigned)");
   }

   {  // std::size_t
      std::string cmd {":1111111\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      resp3::response::storage_type expected
        { {1UL, 0UL, resp3::type::number, {"1111111"}} };
      co_await async_read(ts, buf, gresp);
      check_equal(gresp.raw(), expected, "number (std::size_t)");
   }
}

net::awaitable<void> test_array()
{
   using namespace aedis;
   std::string buf;
   {  // String
      std::string cmd {"*3\r\n$3\r\none\r\n$3\r\ntwo\r\n$5\r\nthree\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      resp3::response::storage_type expected
	 { {3UL, 0UL, resp3::type::array, {}}
	 , {1UL, 1UL, resp3::type::blob_string, {"one"}}
	 , {1UL, 1UL, resp3::type::blob_string, {"two"}}
	 , {1UL, 1UL, resp3::type::blob_string, {"three"}}
         };
      co_await async_read(ts, buf, gresp);
      check_equal(gresp.raw(), expected, "array");
   }

   //{  // int
   //   std::string cmd {"*3\r\n$1\r\n1\r\n$1\r\n2\r\n$1\r\n3\r\n"};
   //   test_tcp_socket ts {cmd};
   //   resp3::flat_array_int_type buffer;
   //   resp3::flat_array_int_adapter res{&buffer};
   //   co_await async_read(ts, buf, res);
   //   check_equal(buffer, {1, 2, 3}, "array (int)");
   //}

   {
      std::string cmd {"*0\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      resp3::response::storage_type expected
	 { {0UL, 0UL, resp3::type::array, {}} };
      co_await async_read(ts, buf, gresp);
      check_equal(gresp.raw(), expected, "array (empty)");
   }
}

net::awaitable<void> test_blob_string()
{
   using namespace aedis;
   std::string buf;
   {
      std::string cmd {"$2\r\nhh\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      resp3::response::storage_type expected
	 { {1UL, 0UL, resp3::type::blob_string, {"hh"}} };
      co_await async_read(ts, buf, gresp);
      check_equal(gresp.raw(), expected, "blob_string");
   }

   {
      std::string cmd {"$26\r\nhhaa\aaaa\raaaaa\r\naaaaaaaaaa\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      resp3::response::storage_type expected
	 { {1UL, 0UL, resp3::type::blob_string, {"hhaa\aaaa\raaaaa\r\naaaaaaaaaa"}} };
      co_await async_read(ts, buf, gresp);
      check_equal(gresp.raw(), expected, "blob_string (with separator)");
   }

   {
      std::string cmd {"$0\r\n\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      resp3::response::storage_type expected
	 { {1UL, 0UL, resp3::type::blob_string, {}} };
      co_await async_read(ts, buf, gresp);
      check_equal(gresp.raw(), expected, "blob_string (size 0)");
   }
}

net::awaitable<void> test_simple_error()
{
   using namespace aedis;
   std::string buf;
   {
      std::string cmd {"-Error\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      resp3::response::storage_type expected
	 { {1UL, 0UL, resp3::type::simple_error, {"Error"}} };
      co_await async_read(ts, buf, gresp);
      check_equal(gresp.raw(), expected, "simple_error (message)");
   }
}

net::awaitable<void> test_floating_point()
{
   using namespace aedis;
   std::string buf;
   {
      std::string cmd {",1.23\r\n"};
      test_tcp_socket ts {cmd};
      resp3::response resp;
      resp3::response::storage_type expected
	 { {1UL, 0UL, resp3::type::doublean, {"1.23"}} };
      co_await async_read(ts, buf, resp);
      check_equal(resp.raw(), expected, "double");
   }

   {
      std::string cmd {",inf\r\n"};
      test_tcp_socket ts {cmd};
      resp3::response resp;
      co_await async_read(ts, buf, resp);
      resp3::response::storage_type expected
	 { {1UL, 0UL, resp3::type::doublean, {"inf"}} };
      check_equal(resp.raw(), expected, "double (inf)");
   }

   {
      std::string cmd {",-inf\r\n"};
      test_tcp_socket ts {cmd};
      resp3::response resp;
      co_await async_read(ts, buf, resp);
      resp3::response::storage_type expected
	 { {1UL, 0UL, resp3::type::doublean, {"-inf"}} };
      check_equal(resp.raw(), expected, "double (-inf)");
   }

}

net::awaitable<void> test_boolean()
{
   using namespace aedis;
   std::string buf;
   {
      std::string cmd {"#f\r\n"};
      test_tcp_socket ts {cmd};
      resp3::response resp;
      resp3::response::storage_type expected
	 { {1UL, 0UL, resp3::type::boolean, {"f"}} };

      co_await async_read(ts, buf, resp);
      check_equal(resp.raw(), expected, "bool (false)");
   }

   {
      std::string cmd {"#t\r\n"};
      test_tcp_socket ts {cmd};
      resp3::response resp;
      resp3::response::storage_type expected
	 { {1UL, 0UL, resp3::type::boolean, {"t"}} };

      co_await async_read(ts, buf, resp);
      check_equal(resp.raw(), expected, "bool (true)");
   }
}

net::awaitable<void> test_blob_error()
{
   using namespace aedis;
   std::string buf;
   {
      std::string cmd {"!21\r\nSYNTAX invalid syntax\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      resp3::response::storage_type expected
	 { {1UL, 0UL, resp3::type::blob_error, {"SYNTAX invalid syntax"}} };
      co_await async_read(ts, buf, gresp);
      check_equal(gresp.raw(), expected, "blob_error (message)");
   }

   {
      std::string cmd {"!0\r\n\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      resp3::response::storage_type expected
	 { {1UL, 0UL, resp3::type::blob_error, {}} };

      co_await async_read(ts, buf, gresp);
      check_equal(gresp.raw(), expected, "blob_error (empty message)");
   }
}

net::awaitable<void> test_verbatim_string()
{
   using namespace aedis;
   std::string buf;
   {
      std::string cmd {"=15\r\ntxt:Some string\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      resp3::response::storage_type expected
	 { {1UL, 0UL, resp3::type::verbatim_string, {"txt:Some string"}} };
      co_await async_read(ts, buf, gresp);
      check_equal(gresp.raw(), expected, "verbatim_string");
   }

   {
      std::string cmd {"=0\r\n\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      co_await async_read(ts, buf, gresp);
      resp3::response::storage_type expected
	 { {1UL, 0UL, resp3::type::verbatim_string, {}} };
      check_equal(gresp.raw(), expected, "verbatim_string (empty)");
   }
}

net::awaitable<void> test_set2()
{
   using namespace aedis;
   std::string buf;
   {
      std::string cmd {"~5\r\n+orange\r\n+apple\r\n+one\r\n+two\r\n+three\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();

      resp3::response::storage_type expected
      { { 5UL, 0UL, resp3::type::set,    {}}
      , { 1UL, 1UL, resp3::type::simple_string, {"orange"}}
      , { 1UL, 1UL, resp3::type::simple_string, {"apple"}}
      , { 1UL, 1UL, resp3::type::simple_string, {"one"}}
      , { 1UL, 1UL, resp3::type::simple_string, {"two"}}
      , { 1UL, 1UL, resp3::type::simple_string, {"three"}}
      };

      co_await async_read(ts, buf, gresp);
      check_equal(gresp.raw(), expected, "test set (1)");
   }

   {
      std::string cmd {"~0\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();

      resp3::response::storage_type expected
      { { 0UL, 0UL, resp3::type::set, {}}
      };

      co_await async_read(ts, buf, gresp);
      check_equal(gresp.raw(), expected, "test set (2)");
   }
}

net::awaitable<void> test_map()
{
   using namespace aedis;
   std::string buf;
   {
      std::string cmd {"%7\r\n$6\r\nserver\r\n$5\r\nredis\r\n$7\r\nversion\r\n$5\r\n6.0.9\r\n$5\r\nproto\r\n:3\r\n$2\r\nid\r\n:203\r\n$4\r\nmode\r\n$10\r\nstandalone\r\n$4\r\nrole\r\n$6\r\nmaster\r\n$7\r\nmodules\r\n*0\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      co_await async_read(ts, buf, gresp);

      resp3::response::storage_type expected
      { {7UL, 0UL, resp3::type::map,    {}}
      , {1UL, 1UL, resp3::type::blob_string, {"server"}}
      , {1UL, 1UL, resp3::type::blob_string, {"redis"}}
      , {1UL, 1UL, resp3::type::blob_string, {"version"}}
      , {1UL, 1UL, resp3::type::blob_string, {"6.0.9"}}
      , {1UL, 1UL, resp3::type::blob_string, {"proto"}}
      , {1UL, 1UL, resp3::type::number, {"3"}}
      , {1UL, 1UL, resp3::type::blob_string, {"id"}}
      , {1UL, 1UL, resp3::type::number, {"203"}}
      , {1UL, 1UL, resp3::type::blob_string, {"mode"}}
      , {1UL, 1UL, resp3::type::blob_string, {"standalone"}}
      , {1UL, 1UL, resp3::type::blob_string, {"role"}}
      , {1UL, 1UL, resp3::type::blob_string, {"master"}}
      , {1UL, 1UL, resp3::type::blob_string, {"modules"}}
      , {0UL, 1UL, resp3::type::array, {}}
      };
      check_equal(gresp.raw(), expected, "test map");
   }

   {
      std::string cmd {"%0\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      co_await async_read(ts, buf, gresp);
      resp3::response::storage_type expected
      { {0UL, 0UL, resp3::type::map, {}} };
      check_equal(gresp.raw(), expected, "test map (empty)");
   }
}

net::awaitable<void> test_streamed_string()
{
   using namespace aedis;
   std::string buf;
   {
      std::string cmd {"$?\r\n;4\r\nHell\r\n;5\r\no wor\r\n;1\r\nd\r\n;0\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      resp3::response::storage_type expected
	 { {1UL, 0UL, resp3::type::streamed_string_part, {"Hello world"}} };
      co_await async_read(ts, buf, gresp);
      check_equal(gresp.raw(), expected, "streamed string");
   }

   {
      std::string cmd {"$?\r\n;0\r\n"};
      test_tcp_socket ts {cmd};
      resp3::response resp;
      co_await async_read(ts, buf, resp);

      resp3::response::storage_type expected
	 { {1UL, 0UL, resp3::type::streamed_string_part, {}} };
      check_equal(resp.raw(), expected, "streamed string (empty)");
   }
}

net::awaitable<void> offline()
{
   std::string buf;
   //{
   //   std::string cmd {"|1\r\n+key-popularity\r\n%2\r\n$1\r\na\r\n,0.1923\r\n$1\r\nb\r\n,0.0012\r\n"};
   //   test_tcp_socket ts {cmd};
   //   resp3::flat_radapter res;
   //   co_await async_read(ts, buf, res);
   //   check_equal(res.result, {"key-popularity", "a", "0.1923", "b", "0.0012"}, "attribute");
   //}

   //{
   //   std::string cmd {">4\r\n+pubsub\r\n+message\r\n+foo\r\n+bar\r\n"};
   //   test_tcp_socket ts {cmd};
   //   resp3::flat_radapter res;
   //   co_await async_read(ts, buf, res);
   //   check_equal(res.result, {"pubsub", "message", "foo", "bar"}, "push type");
   //}

   //{
   //   std::string cmd {">0\r\n"};
   //   test_tcp_socket ts {cmd};
   //   resp3::flat_radapter res;
   //   co_await async_read(ts, buf, res);
   //   check_equal(res.result, {}, "push type (empty)");
   //}
}

int main(int argc, char* argv[])
{
   net::io_context ioc {1};
   tcp::resolver resv(ioc);
   auto const res = resv.resolve("127.0.0.1", "6379");

   co_spawn(ioc, test_simple_string(), net::detached);
   co_spawn(ioc, test_number(), net::detached);
   co_spawn(ioc, test_array(), net::detached);
   co_spawn(ioc, test_blob_string(), net::detached);
   co_spawn(ioc, test_simple_error(), net::detached);
   co_spawn(ioc, test_floating_point(), net::detached);
   co_spawn(ioc, test_boolean(), net::detached);
   co_spawn(ioc, test_blob_error(), net::detached);
   co_spawn(ioc, test_verbatim_string(), net::detached);
   co_spawn(ioc, test_set2(), net::detached);
   co_spawn(ioc, test_map(), net::detached);

   //co_spawn(ioc, test_list(res), net::detached);
   co_spawn(ioc, test_set(res), net::detached);
   co_spawn(ioc, test_general(res), net::detached);
   ioc.run();
}

