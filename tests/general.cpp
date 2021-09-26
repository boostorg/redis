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
      auto const type =
        co_await cs.async_consume(socket, requests, resp, net::use_awaitable);

      if (type == resp3::type::flat_push) {
	 switch (push_counter) {
	    case 0: check_equal(resp.flat_push(), {"subscribe", "channel", "1"}, "push (value1)"); break;
	    case 1: check_equal(resp.flat_push(), {"message", "channel", "message"}, "push (value2)"); break;
	    default: std::cout << "ERROR: unexpected push event." << std::endl;
	 }
	 ++push_counter;
         continue;
      }

      auto const cmd = requests.front().commands.front();

      switch (cmd) {
	 case command::hello:
	 {
	    prepare_next(requests);
	    filler(requests.back());
	 } break;
	 case command::multi: check_equal(resp.simple_string(), {"OK"}, "multi"); break;
	 case command::ping: check_equal(resp.simple_string(), {"QUEUED"}, "ping"); break;
	 case command::set: check_equal(resp.simple_string(), {"OK"}, "set"); break;
	 case command::quit: check_equal(resp.simple_string(), {"OK"}, "quit"); break;
	 case command::flushall: check_equal(resp.simple_string(), {"OK"}, "flushall"); break;
	 case command::ltrim: check_equal(resp.simple_string(), {"OK"}, "ltrim"); break;
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
	 case command::get: check_equal(resp.blob_string(), filler.set_, "get"); break;
	 case command::hget: check_equal(resp.blob_string(), std::string{"value2"}, "hget"); break;
	 case command::lrange: check_equal(resp.flat_array(), {"1", "2", "3", "4", "5", "6"}, "lrange"); break;
	 case command::hvals: check_equal(resp.flat_array(), {"value1", "value2"}, "hvals"); break;
	 case command::zrange: check_equal(resp.flat_array(), {"Marcelo"}, "hvals"); break;
	 case command::zrangebyscore: check_equal(resp.flat_array(), {"Marcelo"}, "zrangebyscore"); break;
	 case command::lpop:
	 {
	     switch (type)
	     {
		case resp3::type::blob_string: check_equal(resp.blob_string(), {"3"}, "lpop"); break;
		case resp3::type::flat_array: check_equal(resp.flat_array(), {"4", "5"}, "lpop"); break;
		default: {std::cout << "Error." << std::endl;}
	     }
	 } break;
	 case command::exec:
	 {
	    check_equal_number(type, resp3::type::flat_array, "exec (type)");
	    check_equal(std::size(resp.array()), 2lu, "exec (size)");

	    check_equal(resp.array()[0].cmd, command::unknown, "transaction ping (command)");
	    check_equal(resp.array()[0].depth, 1, "transaction (depth)");
	    check_equal(resp.array()[0].type, resp3::type::simple_string, "transaction (type)");
	    check_equal(resp.array()[0].expected_size, 1, "transaction (size)");

	    check_equal(resp.array()[1].cmd, command::unknown, "transaction ping (command)");
	    check_equal(resp.array()[1].depth, 1, "transaction (depth)");
	    check_equal(resp.array()[1].type, resp3::type::simple_string, "transaction (typ)e");
	    check_equal(resp.array()[1].expected_size, 1, "transaction (size)");
	 } break;
	 case command::hgetall: check_equal(resp.flat_map(), {"field1", "value1", "field2", "value2"}, "hgetall (value)"); break;
	 case command::smembers: check_equal(resp.flat_set(), {"1", "2", "3"}, "smembers (value)"); break;
	 default: { std::cout << "Error: " << type << " " << cmd << std::endl; }
      }

      resp.blob_string().clear();
      resp.flat_push().clear();
      resp.flat_map().clear();
      resp.flat_set().clear();
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
      resp3::simple_string_type buffer;
      resp3::detail::simple_string_adapter res{&buffer};
      co_await async_read_one(socket, buf, res);
      check_equal(buffer, {"OK"}, "flushall");
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
      resp3::simple_string_type buffer;
      resp3::detail::simple_string_adapter res{&buffer};
      co_await async_read_one(socket, buf, res);
      check_equal(buffer, {"OK"}, "ltrim");
   }

   {  // lpop. Why a blob string instead of a number?
      resp3::blob_string_type buffer;
      resp3::detail::blob_string_adapter res{&buffer};
      co_await async_read_one(socket, buf, res);
      check_equal(buffer, {"3"}, "lpop");
   }

   {  // quit
      resp3::simple_string_type buffer;
      resp3::detail::simple_string_adapter res{&buffer};
      co_await async_read_one(socket, buf, res);
      check_equal(buffer, {"OK"}, "quit");
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
      resp3::simple_string_type buffer;
      resp3::detail::simple_string_adapter res{&buffer};
      co_await async_read_one(socket, buf, res);
      check_equal(buffer, {"OK"}, "set1");
   }

   {  // get
      resp3::blob_string_type buffer;
      resp3::detail::blob_string_adapter res{&buffer};
      co_await async_read_one(socket, buf, res);
      check_equal(buffer, test_bulk1, "get1");
   }

   {  // set
      resp3::simple_string_type buffer;
      resp3::detail::simple_string_adapter res{&buffer};
      co_await async_read_one(socket, buf, res);
      check_equal(buffer, {"OK"}, "set1");
   }

   {  // get
      resp3::blob_string_type buffer;
      resp3::detail::blob_string_adapter res{&buffer};
      co_await async_read_one(socket, buf, res);
      check_equal(buffer, test_bulk2, "get2");
   }

   {  // set
      resp3::simple_string_type buffer;
      resp3::detail::simple_string_adapter res{&buffer};
      co_await async_read_one(socket, buf, res);
      check_equal(buffer, {"OK"}, "set3");
   }

   {  // get
      resp3::blob_string_type buffer;
      resp3::detail::blob_string_adapter res{&buffer};
      co_await async_read_one(socket, buf, res);
      check_equal(buffer,  std::string {}, "get3");
   }

   {  // quit
      resp3::simple_string_type buffer;
      resp3::detail::simple_string_adapter res{&buffer};
      co_await async_read_one(socket, buf, res);
      check_equal(buffer, {"OK"}, "quit");
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
      resp3::simple_string_type buffer;
      resp3::detail::simple_string_adapter res{&buffer};
      co_await async_read_one(ts, buf, res);
      check_equal(buffer, {"OK"}, "simple_string");
      //check_equal(res.attribute.value, {}, "simple_string (empty attribute)");
   }

   {  // empty
      std::string buf;
      std::string cmd {"+\r\n"};
      test_tcp_socket ts {cmd};
      resp3::simple_string_type buffer;
      resp3::detail::simple_string_adapter res{&buffer};
      co_await async_read_one(ts, buf, res);
      check_equal(buffer, {}, "simple_string (empty)");
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
      resp3::blob_string_type buffer;
      resp3::detail::blob_string_adapter res{&buffer};
      co_await async_read_one(ts, buf, res);
      check_equal(buffer, {"hh"}, "blob_string");
   }

   {
      std::string cmd {"$26\r\nhhaa\aaaa\raaaaa\r\naaaaaaaaaa\r\n"};
      test_tcp_socket ts {cmd};
      resp3::blob_string_type buffer;
      resp3::detail::blob_string_adapter res{&buffer};
      co_await async_read_one(ts, buf, res);
      check_equal(buffer, {"hhaa\aaaa\raaaaa\r\naaaaaaaaaa"}, "blob_string (with separator)");
   }

   {
      std::string cmd {"$0\r\n\r\n"};
      test_tcp_socket ts {cmd};
      resp3::blob_string_type buffer;
      resp3::detail::blob_string_adapter res{&buffer};
      co_await async_read_one(ts, buf, res);
      check_equal(buffer, {}, "blob_string (size 0)");
   }
}

net::awaitable<void> test_simple_error()
{
   using namespace aedis;
   std::string buf;
   {
      std::string cmd {"-Error\r\n"};
      test_tcp_socket ts {cmd};
      resp3::simple_error_type buffer;
      resp3::detail::simple_error_adapter res{&buffer};
      co_await async_read_one(ts, buf, res);
      check_equal(buffer, {"Error"}, "simple_error (message)");
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
      resp3::flat_set_type buffer;
      resp3::detail::flat_set_adapter res{&buffer};
      co_await async_read_one(ts, buf, res);
      check_equal(buffer, {"orange", "apple", "one", "two", "three"}, "set");
   }

   {
      std::string cmd {"~5\r\n+orange\r\n+apple\r\n+one\r\n+two\r\n+three\r\n"};
      test_tcp_socket ts {cmd};
      resp3::flat_set_type buffer;
      resp3::detail::flat_set_adapter res{&buffer};
      co_await async_read_one(ts, buf, res);
      check_equal(buffer, {"orange", "apple", "one", "two", "three"}, "set (flat)");
   }

   {
      std::string cmd {"~0\r\n"};
      test_tcp_socket ts {cmd};
      resp3::flat_set_type buffer;
      resp3::detail::flat_set_adapter res{&buffer};
      co_await async_read_one(ts, buf, res);
      check_equal(buffer, {}, "set (empty)");
   }
}

net::awaitable<void> test_map()
{
   using namespace aedis;
   std::string buf;
   {
      std::string cmd {"%7\r\n$6\r\nserver\r\n$5\r\nredis\r\n$7\r\nversion\r\n$5\r\n6.0.9\r\n$5\r\nproto\r\n:3\r\n$2\r\nid\r\n:203\r\n$4\r\nmode\r\n$10\r\nstandalone\r\n$4\r\nrole\r\n$6\r\nmaster\r\n$7\r\nmodules\r\n*0\r\n"};
      test_tcp_socket ts {cmd};
      resp3::flat_map_type buffer;
      resp3::detail::flat_map_adapter res{&buffer};
      co_await async_read_one(ts, buf, res);
      check_equal(buffer, {"server", "redis", "version", "6.0.9", "proto", "3", "id", "203", "mode", "standalone", "role", "master", "modules"}, "map (flat)");
   }

   {
      std::string cmd {"%0\r\n"};
      test_tcp_socket ts {cmd};
      resp3::flat_map_type buffer;
      resp3::detail::flat_map_adapter res{&buffer};
      co_await async_read_one(ts, buf, res);
      check_equal(buffer, {}, "map (flat - empty)");
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
      auto* adapter = resp.select_adapter(resp3::type::streamed_string_part, command::unknown);
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

