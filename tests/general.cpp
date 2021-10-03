/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/aedis.hpp>

#include "test_stream.hpp"

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
using flat_push_adapter = detail::basic_flat_array_adapter<std::string>;

} // resp3
} // aedis

using namespace aedis;

resp3::array_type array_buffer;
resp3::detail::array_adapter array_adapter{&array_buffer};

template <class T>
void check_equal(T const& a, T const& b, std::string const& msg = "")
{
   if (a == b)
     std::cout << "Success: " << msg << std::endl;
   else
     std::cout << "Error: " << msg << std::endl;
}

template <class T>
void check_equal_number(T const& a, T const& b, std::string const& msg = "")
{
   if (a == b)
     std::cout << "Success: " << msg << std::endl;
   else
     std::cout << "Error: " << a << " != " << b << " " << msg << std::endl;
}

//-------------------------------------------------------------------

struct test_general_fill {
   std::vector<int> list_ {1 ,2, 3, 4, 5, 6};
   std::string set_ {"aaa"};

   void operator()(resp3::request& p) const
   {
      p.flushall();
      p.rpush("a", list_);
      p.llen("a");
      p.lrange("a");
      p.ltrim("a", 2, -2);
      p.lpop("a");
      //p.lpop("a", 2); // Not working?
      p.set("b", {set_});
      p.get("b");
      p.append("b", "b");
      p.del("b");
      p.subscribe("channel");
      p.publish("channel", "message");
      p.incr("c");

      //----------------------------------
      // transaction
      for (auto i = 0; i < 3; ++i) {
	 p.multi();
	 p.ping();
	 p.lrange("a");
	 p.ping();
	 // TODO: It looks like we can't publish to a channel we
	 // are already subscribed to from inside a transaction.
	 //req.publish("some-channel", "message1");
	 p.exec();
      }
      //----------------------------------

      std::map<std::string, std::string> m1 =
      { {"field1", "value1"}
      , {"field2", "value2"}};

      p.hset("d", m1);
      p.hget("d", "field2");
      p.hgetall("d");
      p.hdel("d", {"field1", "field2"});
      p.hincrby("e", "some-field", 10);

      p.zadd("f", 1, "Marcelo");
      p.zrange("f");
      p.zrangebyscore("f", 1, 1);
      p.zremrangebyscore("f", "-inf", "+inf");

      p.sadd("g", std::vector<int>{1, 2, 3});
      p.smembers("g");

      p.quit();
   }
};

net::awaitable<void>
test_general(net::ip::tcp::resolver::results_type const& res)
{
   auto ex = co_await this_coro::executor;
   net::ip::tcp::socket socket{ex};
   co_await net::async_connect(socket, res, net::use_awaitable);

   std::queue<resp3::request> requests;
   requests.push({});
   requests.back().hello("3");

   test_general_fill filler;

   resp3::response resp;
   resp3::consumer cs;

   int push_counter = 0;
   for (;;) {
      auto const t =
        co_await cs.async_consume(socket, requests, resp, net::use_awaitable);

      if (t == resp3::type::flat_push) {
	 switch (push_counter) {
	    case 0: check_equal(resp.flat_push(), {"subscribe", "channel", "1"}, "push (value1)"); break;
	    case 1: check_equal(resp.flat_push(), {"message", "channel", "message"}, "push (value2)"); break;
	    default: std::cout << "ERROR: unexpected push event." << std::endl;
	 }
	 ++push_counter;
         continue;
      }

      auto const id = requests.front().ids.front();

      switch (id.first) {
	 case command::hello:
	 {
	    prepare_next(requests);
	    filler(requests.back());
	 } break;
	 case command::multi:
	 {
	    resp3::array_type expected
	    { {1UL, 0UL, resp3::type::simple_string, {"OK"}} };

	    check_equal(resp.array(), expected, "multi");
	 } break;
	 case command::ping:
	 {
	    resp3::array_type expected
	    { {1UL, 0UL, resp3::type::simple_string, {"QUEUED"}} };

	    check_equal(resp.array(), expected, "ping");
	 } break;
	 case command::set:
	 {
	    resp3::array_type expected
	    { {1UL, 0UL, resp3::type::simple_string, {"OK"}} };

	    check_equal(resp.array(), expected, "set");
	 } break;
	 case command::quit:
	 {
	    resp3::array_type expected
	    { {1UL, 0UL, resp3::type::simple_string, {"OK"}} };

	    check_equal(resp.array(), expected, "quit");
	 } break;
	 case command::flushall:
	 {
	    resp3::array_type expected
	    { {1UL, 0UL, resp3::type::simple_string, {"OK"}} };

	    check_equal(resp.array(), expected, "flushall");
	 } break;
	 case command::ltrim:
	 {
	    resp3::array_type expected
	    { {1UL, 0UL, resp3::type::simple_string, {"OK"}} };

	    check_equal(resp.array(), expected, "ltrim");
	 } break;
	 case command::append: check_equal(resp.number(), 4LL, "append"); break;
	 case command::hset: check_equal(resp.number(), 2LL, "hset"); break;
	 case command::rpush: check_equal(resp.number(), (resp3::number_type)std::size(filler.list_), "rpush (value)"); break;
	 case command::del: check_equal(resp.number(), 1LL, "del"); break;
	 case command::llen: check_equal(resp.number(), 6LL, "llen"); break;
	 case command::incr: check_equal(resp.number(), 1LL, "incr"); break;
	 case command::publish: check_equal(resp.number(), 1LL, "publish"); break;
	 case command::hincrby: check_equal(resp.number(), 10LL, "hincrby"); break;
	 case command::zadd: check_equal(resp.number(), 1LL, "zadd"); break;
	 case command::sadd: check_equal(resp.number(), 3LL, "sadd"); break;
	 case command::hdel: check_equal(resp.number(), 2LL, "hdel"); break;
	 case command::zremrangebyscore: check_equal(resp.number(), 1LL, "zremrangebyscore"); break;
	 case command::get:
	 {
	    resp3::array_type expected
	       { {1UL, 0UL, resp3::type::blob_string, filler.set_} };

	    check_equal(resp.array(), expected, "get");
	 } break;
	 case command::hget:
	 {
	    resp3::array_type expected
	       { {1UL, 0UL, resp3::type::blob_string, std::string{"value2"}} };

	    check_equal(resp.array(), expected, "hget");
	 } break;
	 case command::lrange: check_equal(resp.flat_array(), {"1", "2", "3", "4", "5", "6"}, "lrange"); break;
	 case command::hvals: check_equal(resp.flat_array(), {"value1", "value2"}, "hvals"); break;
	 case command::zrange: check_equal(resp.flat_array(), {"Marcelo"}, "hvals"); break;
	 case command::zrangebyscore: check_equal(resp.flat_array(), {"Marcelo"}, "zrangebyscore"); break;
	 case command::lpop:
	 {
	    switch (t)
	    {
	       case resp3::type::blob_string:
	       {
	          resp3::array_type expected
		     { {1UL, 0UL, resp3::type::blob_string, {"3"}} };

	          check_equal(resp.array(), expected, "lpop");
	       } break;
	       case resp3::type::flat_array: check_equal(resp.flat_array(), {"4", "5"}, "lpop"); break;
	       default: {std::cout << "Error." << std::endl;}
	    }
	 } break;
	 case command::exec:
	 {
	    check_equal_number(t, resp3::type::flat_array, "exec (type)");
	    check_equal_number(std::size(resp.array()), 6lu, "exec size (description)");

	    check_equal_number(resp.array().at(0).size, 3UL, "(0) transaction (size)");
	    check_equal_number(resp.array().at(0).depth, 0UL, "(0) transaction (depth)");
	    check_equal_number(resp.array().at(0).data_type, resp3::type::flat_array, "(0) transaction (type)");
	    check_equal(resp.array().at(0).data, {}, "(0) transaction (value)");

	    check_equal_number(resp.array().at(1).size, 1UL, "(1) transaction (size)");
	    check_equal_number(resp.array().at(1).depth, 1UL, "(1) transaction (depth)");
	    check_equal_number(resp.array().at(1).data_type, resp3::type::simple_string, "(1) transaction (type)");
	    check_equal(resp.array().at(1).data, {"PONG"}, "(1) transaction (value)");

	    check_equal_number(resp.array().at(2).size, 2UL, "(2) transaction (size)");
	    check_equal_number(resp.array().at(2).depth, 1UL, "(2) transaction (depth)");
	    check_equal_number(resp.array().at(2).data_type, resp3::type::flat_array, "(2) transaction (type)");
	    check_equal(resp.array().at(2).data, {}, "(2) transaction (value)");

	    check_equal_number(resp.array().at(3).size, 1UL, "(3) transaction (size)");
	    check_equal_number(resp.array().at(3).depth, 2UL, "(3) transaction (depth)");
	    check_equal_number(resp.array().at(3).data_type, resp3::type::blob_string, "(3) transaction (type)");
	    check_equal(resp.array().at(3).data, {"4"}, "(3) transaction (value)");

	    check_equal_number(resp.array().at(4).size, 1UL, "(4) transaction (size)");
	    check_equal_number(resp.array().at(4).depth, 2UL, "(4) transaction (depth)");
	    check_equal_number(resp.array().at(4).data_type, resp3::type::blob_string, "(4) transaction (type)");
	    check_equal(resp.array().at(4).data, {"5"}, "(4) transaction (value)");

	    check_equal_number(resp.array().at(5).size, 1UL, "(5) transaction (size)");
	    check_equal_number(resp.array().at(5).depth, 1UL, "(5) transaction (depth)");
	    check_equal_number(resp.array().at(5).data_type, resp3::type::simple_string, "(5) transaction (type)");
	    check_equal(resp.array().at(5).data, {"PONG"}, "(5) transaction (value)");

	 } break;
	 case command::hgetall:
	 {
	    resp3::array_type expected
	    { { 4UL, 0UL, resp3::type::map,    {}}
	    , { 1UL, 1UL, resp3::type::blob_string, {"field1"}}
	    , { 1UL, 1UL, resp3::type::blob_string, {"value1"}}
	    , { 1UL, 1UL, resp3::type::blob_string, {"field2"}}
	    , { 1UL, 1UL, resp3::type::blob_string, {"value2"}}
	    };
	    check_equal(resp.array(), expected, "hgetall (value)");
	 } break;
	 case command::smembers:
	 {
	    resp3::array_type expected
	    { { 3UL, 0UL, resp3::type::set,    {}}
	    , { 1UL, 1UL, resp3::type::blob_string, {"1"}}
	    , { 1UL, 1UL, resp3::type::blob_string, {"2"}}
	    , { 1UL, 1UL, resp3::type::blob_string, {"3"}}
	    };
	    check_equal(resp.array(), expected, "smembers (value)");
	 } break;
	 default: { std::cout << "Error: " << t << " " << id.first << std::endl; }
      }

      resp.flat_push().clear();
      resp.array().clear();
   }
}

//-------------------------------------------------------------------

net::awaitable<void>
test_list(net::ip::tcp::resolver::results_type const& results)
{
   std::vector<int> list {1 ,2, 3, 4, 5, 6};

   resp3::request p;
   p.hello("3");
   p.flushall();
   p.rpush("a", list);
   p.lrange("a");
   p.lrange("a", 2, -2);
   p.ltrim("a", 2, -2);
   p.lpop("a");
   p.quit();

   auto ex = co_await this_coro::executor;
   tcp_socket socket {ex};
   co_await async_connect(socket, results);
   co_await async_write(socket, net::buffer(p.payload));
   std::string buf;

   {  // hello
      resp3::detail::ignore_adapter res;
      co_await async_read_one(socket, buf, res);
   }

   {  // flushall
      array_buffer.clear();
      array_adapter.clear();
      co_await async_read_one(socket, buf, array_adapter);
      resp3::array_type expected
      { {1UL, 0UL, resp3::type::simple_string, {"OK"}} };
      check_equal(array_buffer, expected, "flushall");
   }

   {  // rpush
      resp3::number_type buffer;
      resp3::detail::number_adapter res{&buffer};
      co_await async_read_one(socket, buf, res);
      check_equal(buffer, (long long int)6, "rpush");
   }

   {  // lrange
      resp3::flat_array_int_type buffer;
      resp3::detail::basic_flat_array_adapter<int> res{&buffer};
      co_await async_read_one(socket, buf, res);
      check_equal(buffer, list, "lrange-1");
   }

   {  // lrange
      resp3::flat_array_int_type buffer;
      resp3::detail::basic_flat_array_adapter<int> res{&buffer};
      co_await async_read_one(socket, buf, res);
      check_equal(buffer, std::vector<int>{3, 4, 5}, "lrange-2");
   }

   {  // ltrim
      array_buffer.clear();
      array_adapter.clear();
      resp3::array_type expected
	 { {1UL, 0UL, resp3::type::simple_string, {"OK"}} };

      co_await async_read_one(socket, buf, array_adapter);
      check_equal(array_buffer, expected, "ltrim");
   }

   {  // lpop. Why a blob string instead of a number?
      array_buffer.clear();
      array_adapter.clear();
      resp3::array_type expected
	 { {1UL, 0UL, resp3::type::blob_string, {"3"}} };

      co_await async_read_one(socket, buf, array_adapter);
      check_equal(array_buffer, expected, "lpop");
   }

   {  // quit
      array_buffer.clear();
      array_adapter.clear();
      co_await async_read_one(socket, buf, array_adapter);
      resp3::array_type expected
      { {1UL, 0UL, resp3::type::simple_string, {"OK"}} };
      check_equal(array_buffer, expected, "ltrim");
   }
}

net::awaitable<void>
test_set(net::ip::tcp::resolver::results_type const& results)
{
   using namespace aedis;
   auto ex = co_await this_coro::executor;

   // Tests whether the parser can handle payloads that contain the separator.
   std::string test_bulk1(10000, 'a');
   test_bulk1[30] = '\r';
   test_bulk1[31] = '\n';

   std::string test_bulk2 = "aaaaa";

   tcp_socket socket {ex};
   co_await async_connect(socket, results);

   resp3::request p;
   p.hello("3");
   p.flushall();
   p.set("s", {test_bulk1});
   p.get("s");
   p.set("s", {test_bulk2});
   p.get("s");
   p.set("s", {""});
   p.get("s");
   p.quit();

   co_await async_write(socket, net::buffer(p.payload));

   std::string buf;
   {  // hello, flushall
      resp3::detail::ignore_adapter res;
      co_await async_read_one(socket, buf, res);
      co_await async_read_one(socket, buf, res);
   }

   {  // set
      array_buffer.clear();
      array_adapter.clear();
      co_await async_read_one(socket, buf, array_adapter);
      resp3::array_type expected
      { {1UL, 0UL, resp3::type::simple_string, {"OK"}} };
      check_equal(array_buffer, expected, "set1");
   }

   {  // get
      array_buffer.clear();
      array_adapter.clear();
      resp3::array_type expected
	 { {1UL, 0UL, resp3::type::blob_string, test_bulk1} };

      co_await async_read_one(socket, buf, array_adapter);
      check_equal(array_buffer, expected, "get1");
   }

   {  // set
      array_buffer.clear();
      array_adapter.clear();
      co_await async_read_one(socket, buf, array_adapter);
      resp3::array_type expected
      { {1UL, 0UL, resp3::type::simple_string, {"OK"}} };
      check_equal(array_buffer, expected, "ltrim");
   }

   {  // get
      array_buffer.clear();
      array_adapter.clear();
      resp3::array_type expected
	 { {1UL, 0UL, resp3::type::blob_string, test_bulk2} };
      co_await async_read_one(socket, buf, array_adapter);
      check_equal(array_buffer, expected, "get2");
   }

   {  // set
      array_buffer.clear();
      array_adapter.clear();
      co_await async_read_one(socket, buf, array_adapter);
      resp3::array_type expected
      { {1UL, 0UL, resp3::type::simple_string, {"OK"}} };
      check_equal(array_buffer, expected, "set3");
   }

   {  // get
      array_buffer.clear();
      array_adapter.clear();
      resp3::array_type expected
	 { {1UL, 0UL, resp3::type::blob_string, {}} };

      co_await async_read_one(socket, buf, array_adapter);
      check_equal(array_buffer,  expected, "get3");
   }

   {  // quit
      array_buffer.clear();
      array_adapter.clear();
      co_await async_read_one(socket, buf, array_adapter);
      resp3::array_type expected
      { {1UL, 0UL, resp3::type::simple_string, {"OK"}} };
      check_equal(array_buffer, expected, "quit");
   }
}

struct test_handler {
   void operator()(boost::system::error_code ec) const
   {
      if (ec)
         std::cout << ec.message() << std::endl;
   }
};

net::awaitable<void> test_simple_string()
{
   using namespace aedis;
   {  // Small string
      std::string buf;
      std::string cmd {"+OK\r\n"};
      test_tcp_socket ts {cmd};
      array_buffer.clear();
      array_adapter.clear();
      co_await async_read_one(ts, buf, array_adapter);
      resp3::array_type expected
      { {1UL, 0UL, resp3::type::simple_string, {"OK"}} };
      check_equal(array_buffer, expected, "simple_string");
   }

   {  // empty
      std::string buf;
      std::string cmd {"+\r\n"};
      test_tcp_socket ts {cmd};
      array_buffer.clear();
      array_adapter.clear();
      co_await async_read_one(ts, buf, array_adapter);
      resp3::array_type expected
      { {1UL, 0UL, resp3::type::simple_string, {}} };
      check_equal(array_buffer, expected, "simple_string (empty)");
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
   //   co_await async_read_one(ts, buffer, res);
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
      resp3::number_type buffer;
      resp3::detail::number_adapter res{&buffer};
      co_await async_read_one(ts, buf, res);
      check_equal(buffer, (long long int)-3, "number (int)");
   }

   {  // unsigned
      std::string cmd {":3\r\n"};
      test_tcp_socket ts {cmd};
      resp3::number_type buffer;
      resp3::detail::number_adapter res{&buffer};
      co_await async_read_one(ts, buf, res);
      check_equal(buffer, (long long int)3, "number (unsigned)");
   }

   {  // std::size_t
      std::string cmd {":1111111\r\n"};
      test_tcp_socket ts {cmd};
      resp3::number_type buffer;
      resp3::detail::number_adapter res{&buffer};
      co_await async_read_one(ts, buf, res);
      check_equal(buffer, (long long int)1111111, "number (std::size_t)");
   }
}

net::awaitable<void> test_array()
{
   using namespace aedis;
   std::string buf;
   {  // String
      std::string cmd {"*3\r\n$3\r\none\r\n$3\r\ntwo\r\n$5\r\nthree\r\n"};
      test_tcp_socket ts {cmd};
      resp3::flat_array_type buffer;
      resp3::flat_array_adapter res{&buffer};
      co_await async_read_one(ts, buf, res);
      check_equal(buffer, {"one", "two", "three"}, "array (dynamic)");
   }

   {  // int
      std::string cmd {"*3\r\n$1\r\n1\r\n$1\r\n2\r\n$1\r\n3\r\n"};
      test_tcp_socket ts {cmd};
      resp3::flat_array_int_type buffer;
      resp3::flat_array_int_adapter res{&buffer};
      co_await async_read_one(ts, buf, res);
      check_equal(buffer, {1, 2, 3}, "array (int)");
   }

   {
      std::string cmd {"*0\r\n"};
      test_tcp_socket ts {cmd};
      resp3::flat_array_type buffer;
      resp3::flat_array_adapter res{&buffer};
      co_await async_read_one(ts, buf, res);
      check_equal(buffer, {}, "array (empty)");
   }
}

net::awaitable<void> test_blob_string()
{
   using namespace aedis;
   std::string buf;
   {
      std::string cmd {"$2\r\nhh\r\n"};
      test_tcp_socket ts {cmd};
      array_buffer.clear();
      array_adapter.clear();
      resp3::array_type expected
	 { {1UL, 0UL, resp3::type::blob_string, {"hh"}} };
      co_await async_read_one(ts, buf, array_adapter);
      check_equal(array_buffer, expected, "blob_string");
   }

   {
      std::string cmd {"$26\r\nhhaa\aaaa\raaaaa\r\naaaaaaaaaa\r\n"};
      test_tcp_socket ts {cmd};
      array_buffer.clear();
      array_adapter.clear();
      resp3::array_type expected
	 { {1UL, 0UL, resp3::type::blob_string, {"hhaa\aaaa\raaaaa\r\naaaaaaaaaa"}} };
      co_await async_read_one(ts, buf, array_adapter);
      check_equal(array_buffer, expected, "blob_string (with separator)");
   }

   {
      std::string cmd {"$0\r\n\r\n"};
      test_tcp_socket ts {cmd};
      array_buffer.clear();
      array_adapter.clear();
      resp3::array_type expected
	 { {1UL, 0UL, resp3::type::blob_string, {}} };
      co_await async_read_one(ts, buf, array_adapter);
      check_equal(array_buffer, expected, "blob_string (size 0)");
   }
}

net::awaitable<void> test_simple_error()
{
   using namespace aedis;
   std::string buf;
   {
      std::string cmd {"-Error\r\n"};
      test_tcp_socket ts {cmd};
      array_buffer.clear();
      array_adapter.clear();
      resp3::array_type expected
	 { {1UL, 0UL, resp3::type::simple_error, {"Error"}} };
      co_await async_read_one(ts, buf, array_adapter);
      check_equal(array_buffer, expected, "simple_error (message)");
   }
}

net::awaitable<void> test_floating_point()
{
   using namespace aedis;
   std::string buf;
   {
      std::string cmd {",1.23\r\n"};
      test_tcp_socket ts {cmd};
      resp3::doublean_type buffer;
      resp3::detail::doublean_adapter res{&buffer};
      co_await async_read_one(ts, buf, res);
      check_equal(buffer, {"1.23"}, "double");
   }

   {
      std::string cmd {",inf\r\n"};
      test_tcp_socket ts {cmd};
      resp3::doublean_type buffer;
      resp3::detail::doublean_adapter res{&buffer};
      co_await async_read_one(ts, buf, res);
      check_equal(buffer, {"inf"}, "double (inf)");
   }

   {
      std::string cmd {",-inf\r\n"};
      test_tcp_socket ts {cmd};
      resp3::doublean_type buffer;
      resp3::detail::doublean_adapter res{&buffer};
      co_await async_read_one(ts, buf, res);
      check_equal(buffer, {"-inf"}, "double (-inf)");
   }

}

net::awaitable<void> test_boolean()
{
   using namespace aedis;
   std::string buf;
   {
      std::string cmd {"#f\r\n"};
      test_tcp_socket ts {cmd};
      resp3::boolean_type buffer;
      resp3::detail::boolean_adapter res{&buffer};
      co_await async_read_one(ts, buf, res);
      check_equal(buffer, false, "bool (false)");
   }

   {
      std::string cmd {"#t\r\n"};
      test_tcp_socket ts {cmd};
      resp3::boolean_type buffer;
      resp3::detail::boolean_adapter res{&buffer};
      co_await async_read_one(ts, buf, res);
      check_equal(buffer, true, "bool (true)");
   }
}

net::awaitable<void> test_blob_error()
{
   using namespace aedis;
   std::string buf;
   {
      std::string cmd {"!21\r\nSYNTAX invalid syntax\r\n"};
      test_tcp_socket ts {cmd};
      resp3::blob_error_type buffer;
      resp3::detail::blob_error_adapter res{&buffer};
      co_await async_read_one(ts, buf, res);
      check_equal(buffer, {"SYNTAX invalid syntax"}, "blob_error (message)");
   }

   {
      std::string cmd {"!0\r\n\r\n"};
      test_tcp_socket ts {cmd};
      resp3::blob_error_type buffer;
      resp3::detail::blob_error_adapter res{&buffer};
      co_await async_read_one(ts, buf, res);
      check_equal(buffer, {}, "blob_error (empty message)");
   }
}

net::awaitable<void> test_verbatim_string()
{
   using namespace aedis;
   std::string buf;
   {
      std::string cmd {"=15\r\ntxt:Some string\r\n"};
      test_tcp_socket ts {cmd};
      resp3::verbatim_string_type buffer;
      resp3::detail::verbatim_string_adapter res{&buffer};
      co_await async_read_one(ts, buf, res);
      check_equal(buffer, {"txt:Some string"}, "verbatim_string");
   }

   {
      std::string cmd {"=0\r\n\r\n"};
      test_tcp_socket ts {cmd};
      resp3::verbatim_string_type buffer;
      resp3::detail::verbatim_string_adapter res{&buffer};
      co_await async_read_one(ts, buf, res);
      check_equal(buffer, {}, "verbatim_string (empty)");
   }
}

net::awaitable<void> test_set2()
{
   using namespace aedis;
   std::string buf;
   {
      std::string cmd {"~5\r\n+orange\r\n+apple\r\n+one\r\n+two\r\n+three\r\n"};
      test_tcp_socket ts {cmd};
      array_buffer.clear();

      resp3::array_type expected
      { { 5UL, 0UL, resp3::type::set,    {}}
      , { 1UL, 1UL, resp3::type::simple_string, {"orange"}}
      , { 1UL, 1UL, resp3::type::simple_string, {"apple"}}
      , { 1UL, 1UL, resp3::type::simple_string, {"one"}}
      , { 1UL, 1UL, resp3::type::simple_string, {"two"}}
      , { 1UL, 1UL, resp3::type::simple_string, {"three"}}
      };

      co_await async_read_one(ts, buf, array_adapter);
      check_equal(array_buffer, expected, "test set (1)");
   }

   {
      std::string cmd {"~0\r\n"};
      test_tcp_socket ts {cmd};
      array_buffer.clear();

      resp3::array_type expected
      { { 0UL, 0UL, resp3::type::set, {}}
      };

      co_await async_read_one(ts, buf, array_adapter);
      check_equal(array_buffer, expected, "test set (2)");
   }
}

net::awaitable<void> test_map()
{
   using namespace aedis;
   std::string buf;
   {
      std::string cmd {"%7\r\n$6\r\nserver\r\n$5\r\nredis\r\n$7\r\nversion\r\n$5\r\n6.0.9\r\n$5\r\nproto\r\n:3\r\n$2\r\nid\r\n:203\r\n$4\r\nmode\r\n$10\r\nstandalone\r\n$4\r\nrole\r\n$6\r\nmaster\r\n$7\r\nmodules\r\n*0\r\n"};
      test_tcp_socket ts {cmd};
      array_buffer.clear();
      array_adapter.clear();
      co_await async_read_one(ts, buf, array_adapter);

      resp3::array_type expected
      { {14UL, 0UL, resp3::type::map,    {}}
      , { 1UL, 1UL, resp3::type::blob_string, {"server"}}
      , { 1UL, 1UL, resp3::type::blob_string, {"redis"}}
      , { 1UL, 1UL, resp3::type::blob_string, {"version"}}
      , { 1UL, 1UL, resp3::type::blob_string, {"6.0.9"}}
      , { 1UL, 1UL, resp3::type::blob_string, {"proto"}}
      , { 1UL, 1UL, resp3::type::number, {"3"}}
      , { 1UL, 1UL, resp3::type::blob_string, {"id"}}
      , { 1UL, 1UL, resp3::type::number, {"203"}}
      , { 1UL, 1UL, resp3::type::blob_string, {"mode"}}
      , { 1UL, 1UL, resp3::type::blob_string, {"standalone"}}
      , { 1UL, 1UL, resp3::type::blob_string, {"role"}}
      , { 1UL, 1UL, resp3::type::blob_string, {"master"}}
      , { 1UL, 1UL, resp3::type::blob_string, {"modules"}}
      , { 0UL, 1UL, resp3::type::flat_array, {}}
      };
      check_equal(array_buffer, expected, "test map");
   }

   {
      std::string cmd {"%0\r\n"};
      test_tcp_socket ts {cmd};
      array_buffer.clear();
      array_adapter.clear();
      co_await async_read_one(ts, buf, array_adapter);
      resp3::array_type expected
      { {0UL, 0UL, resp3::type::map, {}} };
      check_equal(array_buffer, expected, "test map (empty)");
   }
}

net::awaitable<void> test_streamed_string()
{
   using namespace aedis;
   std::string buf;
   {
      std::string cmd {"$?\r\n;4\r\nHell\r\n;5\r\no wor\r\n;1\r\nd\r\n;0\r\n"};
      test_tcp_socket ts {cmd};
      resp3::streamed_string_part_type buffer;
      resp3::detail::streamed_string_part_adapter res{&buffer};
      co_await resp3::async_read_one(ts, buf, res);
      check_equal(buffer, {"Hello word"}, "streamed string");
   }

   {
      std::string cmd {"$?\r\n;0\r\n"};
      test_tcp_socket ts {cmd};
      resp3::response resp;
      auto* adapter = resp.select_adapter(resp3::type::streamed_string_part, command::unknown, {});
      co_await resp3::async_read_one(ts, buf, *adapter);
      check_equal(resp.streamed_string_part(), {}, "streamed string (empty)");
   }
}

net::awaitable<void> offline()
{
   std::string buf;
   //{
   //   std::string cmd {"|1\r\n+key-popularity\r\n%2\r\n$1\r\na\r\n,0.1923\r\n$1\r\nb\r\n,0.0012\r\n"};
   //   test_tcp_socket ts {cmd};
   //   resp3::flat_array_adapter res;
   //   co_await async_read_one(ts, buf, res);
   //   check_equal(res.result, {"key-popularity", "a", "0.1923", "b", "0.0012"}, "attribute");
   //}

   //{
   //   std::string cmd {">4\r\n+pubsub\r\n+message\r\n+foo\r\n+bar\r\n"};
   //   test_tcp_socket ts {cmd};
   //   resp3::flat_array_adapter res;
   //   co_await async_read_one(ts, buf, res);
   //   check_equal(res.result, {"pubsub", "message", "foo", "bar"}, "push type");
   //}

   //{
   //   std::string cmd {">0\r\n"};
   //   test_tcp_socket ts {cmd};
   //   resp3::flat_array_adapter res;
   //   co_await async_read_one(ts, buf, res);
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

   co_spawn(ioc, test_list(res), net::detached);
   co_spawn(ioc, test_set(res), net::detached);
   co_spawn(ioc, test_general(res), net::detached);
   ioc.run();
}

