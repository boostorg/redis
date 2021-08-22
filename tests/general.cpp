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

net::awaitable<void>
test_general(net::ip::tcp::resolver::results_type const& res)
{
   auto ex = co_await this_coro::executor;

   net::ip::tcp::socket socket{ex};
   co_await net::async_connect(socket, res, net::use_awaitable);

   std::queue<pipeline> reqs;
   response_buffers bufs;
   std::string buffer;

   prepare_queue(reqs);
   reqs.back().hello("3");

   std::vector<int> list_ {1 ,2, 3, 4, 5, 6};
   std::string set_ {"aaa"};

   co_await async_write(socket, net::buffer(reqs.back().payload), net::use_awaitable);

   detail::response_adapters adapters{bufs};
   for (;;) {
      auto const event = co_await async_consume(socket, buffer, adapters, reqs);

      switch (event.first) {
	 case command::hello:
	 {
	    bufs.map.clear();
	    auto const empty = prepare_queue(reqs);
	    reqs.back().flushall();
	    reqs.back().rpush("a", list_);
	    reqs.back().llen("a");
	    reqs.back().lrange("a");
	    reqs.back().ltrim("a", 2, -2);
	    reqs.back().lpop("a");
	    //reqs.back().lpop("a", 2); // Not working?
	    reqs.back().set("b", {set_});
	    reqs.back().get("b");
	    reqs.back().append("b", "b");
	    reqs.back().del("b");
	    reqs.back().subscribe("channel");
	    reqs.back().publish("channel", "message");
	    reqs.back().incr("c");

	    std::map<std::string, std::string> m1 =
	    { {"field1", "value1"}
	    , {"field2", "value2"}};

	    reqs.back().hset("d", m1);
	    reqs.back().hget("d", "field2");
	    reqs.back().hgetall("d");
	    reqs.back().hdel("d", {"field1", "field2"});
	    reqs.back().hincrby("e", "some-field", 10);

	    reqs.back().zadd("f", 1, "Marcelo");
	    reqs.back().zrange("f");
	    reqs.back().zrangebyscore("f", 1, 1);
	    reqs.back().zremrangebyscore("f", "-inf", "+inf");

	    reqs.back().sadd("g", std::vector<int>{1, 2, 3});
	    reqs.back().smembers("g");

	    reqs.back().quit();
	    if (empty)
	       co_await async_write_all(socket, reqs);
	 } break;
	 case command::get:
	    check_equal(event.second, resp3::type::blob_string, "get (type)");
	    check_equal(bufs.blob_string, set_, "get (value)");
	    bufs.blob_string.clear();
	    break;
	 case command::lrange:
	    check_equal(event.second, resp3::type::array, "lrange (type)");
	    check_equal(bufs.array, {"1", "2", "3", "4", "5", "6"}, "lrange (value)");
	    bufs.array.clear();
	    break;
	 case command::hget:
	    check_equal(event.second, resp3::type::blob_string, "hget (type)");
	    check_equal(bufs.blob_string, std::string{"value2"}, "hget (value)");
	    bufs.blob_string.clear();
	    break;
	 case command::hgetall:
	    check_equal(event.second, resp3::type::map, "hgetall (type)");
	    check_equal(bufs.map, {"field1", "value1", "field2", "value2"}, "hgetall (value)");
	    bufs.map.clear();
	    break;
	 case command::hvals:
	    check_equal(event.second, resp3::type::array, "hvals (type)");
	    check_equal(bufs.array, {"value1", "value2"}, "hvals (value)");
	    bufs.array.clear();
	    break;
	 case command::zrange:
	    check_equal(event.second, resp3::type::array, "hvals (type)");
	    check_equal(bufs.array, {"Marcelo"}, "hvals (value)");
	    bufs.array.clear();
	    break;
	 case command::zrangebyscore:
	    check_equal(event.second, resp3::type::array, "zrangebyscore (type)");
	    check_equal(bufs.array, {"Marcelo"}, "zrangebyscore (value)");
	    bufs.array.clear();
	    break;
	 case command::smembers:
	    check_equal(event.second, resp3::type::set, "smembers (type)");
	    check_equal(bufs.set, {"1", "2", "3"}, "smembers (value)");
	    bufs.set.clear();
	    break;
	 case command::set:
	    check_equal(event.second, resp3::type::simple_string, "set (type)");
	    check_equal(bufs.simple_string, {"OK"}, "set (value)");
	    bufs.simple_string.clear();
	    break;
	 case command::quit:
	    check_equal(event.second, resp3::type::simple_string, "quit (type)");
	    check_equal(bufs.simple_string, {"OK"}, "quit (value)");
	    bufs.simple_string.clear();
	    break;
	 case command::flushall:
	    check_equal(event.second, resp3::type::simple_string, "flushall (type)");
	    check_equal(bufs.simple_string, {"OK"}, "flushall (value)");
	    bufs.simple_string.clear();
	    break;
	 case command::ltrim:
	    check_equal(event.second, resp3::type::simple_string, "ltrim (type)");
	    check_equal(bufs.simple_string, {"OK"}, "ltrim (value)");
	    bufs.simple_string.clear();
	    break;
	 case command::append:
	    check_equal(event.second, resp3::type::number, "append (type)");
	    check_equal(bufs.number, 4LL, "append (value)");
	    break;
	 case command::hset:
	    check_equal(event.second, resp3::type::number, "hset (type)");
	    check_equal(bufs.number, 2LL, "hset (value)");
	    break;
	 case command::rpush:
	    check_equal(event.second, resp3::type::number, "rpush (type)");
	    check_equal(bufs.number, (resp3::number)std::size(list_), "rpush (value)");
	    break;
	 case command::del:
	    check_equal(event.second, resp3::type::number, "del (type)");
	    check_equal(bufs.number, 1LL, "del (value)");
	    break;
	 case command::llen:
	    check_equal(event.second, resp3::type::number, "llen (type)");
	    check_equal(bufs.number, 6LL, "llen (value)");
	    break;
	 case command::incr:
	    check_equal(event.second, resp3::type::number, "incr (type)");
	    check_equal(bufs.number, 1LL, "incr (value)");
	    break;
	 case command::publish:
	    check_equal(event.second, resp3::type::number, "publish (type)");
	    check_equal(bufs.number, 1LL, "publish (value)");
	    break;
	 case command::hincrby:
	    check_equal(event.second, resp3::type::number, "hincrby (type)");
	    check_equal(bufs.number, 10LL, "hincrby (value)");
	    break;
	 case command::zadd:
	    check_equal(event.second, resp3::type::number, "zadd (type)");
	    check_equal(bufs.number, 1LL, "zadd (value)");
	    break;
	 case command::sadd:
	    check_equal(event.second, resp3::type::number, "sadd (type)");
	    check_equal(bufs.number, 3LL, "sadd (value)");
	    bufs.simple_string.clear();
	    break;
	 case command::hdel:
	    check_equal(event.second, resp3::type::number, "hdel (type)");
	    check_equal(bufs.number, 2LL, "hdel (value)");
	    bufs.simple_string.clear();
	    break;
	 case command::zremrangebyscore:
	    check_equal(event.second, resp3::type::number, "zremrangebyscore (type)");
	    check_equal(bufs.number, 1LL, "zremrangebyscore (value)");
	    bufs.simple_string.clear();
	    break;
	 case command::lpop:
	 {
	    if (event.second == resp3::type::array) {
	       check_equal(bufs.array, {"4", "5"}, "lpop (count) (value)");
	       bufs.array.clear();
	    } else if (event.second == resp3::type::blob_string) {
	       check_equal(bufs.blob_string, {"3"}, "lpop (value) (value)");
	       bufs.array.clear();
	    } else {
	       std::cout << "Error: in lpop test_general." << std::endl;
	    }
	 } break;
	 default: {
	    check_equal(event.second, resp3::type::push, "push (type)");
	    // TODO: Check the responses below.
	    // {"subscribe", "channel", "1"}
	    // {"message", "channel", "message"}
	    //
	    //std::cout
	    //   << "ERROR: unexpected event in test_general. "
	    //   << event.first << " "
	    //   << event.second << " "
	    //   << std::endl;
	 }
      }
   }
}

//-------------------------------------------------------------------

void trans_filler(auto& req)
{
   req.subscribe("some-channel");
   req.publish("some-channel", "message0");
   req.multi();
   req.ping();
   req.ping();

   // TODO: It looks like we can't publish to a channel we are already
   // subscribed to from inside a transaction.
   //req.publish("some-channel", "message1");

   req.exec();
   req.incr("a");
   req.publish("some-channel", "message2");
};

class trans_receiver : public receiver_base {
private:
   int counter_ = 0;
   std::shared_ptr<connection> conn_;
   response_buffers& buffers_;

public:
   trans_receiver(std::shared_ptr<connection> conn, response_buffers& bufs) : conn_{conn}, buffers_{bufs} { }

   void on_hello(resp3::type type) noexcept override
   {
      auto f = [this](auto& req)
      {
	 req.flushall();
	 trans_filler(req);
      };

      for (auto i = 0; i < 3; ++i) {
	 conn_->send(f);
      }
   }

   void on_push() noexcept override
   {
     auto& v = buffers_.push;
     assert(std::size(v) == 3U);
     
     auto const i = counter_ % 3;
     switch (i) {
       case 0:
       {
          check_equal(v[0], {"subscribe"}, "on_push subscribe (transaction)");
          check_equal(v[1], {"some-channel"}, "on_push (transaction)");
          check_equal(v[2], {"1"}, "on_push (transaction)");
       } break;
       case 1:
       {
          check_equal(v[0], {"message"}, "on_push message (transaction)");
          check_equal(v[1], {"some-channel"}, "on_push (transaction)");
          check_equal(v[2], {"message0"}, "on_push (transaction)");
       } break;
       //case 2: // See not above.
       //{
       //   check_equal(v[0], {"message"}, "on_push message (transaction)");
       //   check_equal(v[1], {"some-channel"}, "on_push (transaction)");
       //   check_equal(v[2], {"message1"}, "on_push (transaction)");
       //} break;
       case 2:
       {
          check_equal(v[0], {"message"}, "on_push message (transaction)");
          check_equal(v[1], {"some-channel"}, "on_push (transaction)");
          check_equal(v[2], {"message2"}, "on_push (transaction)");
       } break;
       defaul: {
         std::cout << "Error: on_push (transaction)" << std::endl;
       }
     }

     ++counter_;
     v.clear();
   }

   void on_flushall(resp3::type type) noexcept override
      { check_equal(buffers_.simple_string, {"OK"}, "flushall (transaction)"); }

   void on_exec(resp3::type type) noexcept override
   {
      check_equal(buffers_.transaction[0].cmd, command::unknown, "transaction ping (command)");
      check_equal(buffers_.transaction[0].depth, 1, "transaction (depth)");
      check_equal(buffers_.transaction[0].type, resp3::type::simple_string, "transaction (type)");
      check_equal(buffers_.transaction[0].expected_size, 1, "transaction (size)");

      check_equal(buffers_.transaction[1].cmd, command::unknown, "transaction ping (command)");
      check_equal(buffers_.transaction[1].depth, 1, "transaction (depth)");
      check_equal(buffers_.transaction[1].type, resp3::type::simple_string, "transaction (typ)e");
      check_equal(buffers_.transaction[1].expected_size, 1, "transaction (size)");

      // See note above
      //check_equal(result[2].command, command::publish, "transaction publish (command)");
      //check_equal_number(result[2].depth, 1, "transaction (depth)");
      //check_equal_number(result[2].type, resp3::type::number, "transaction (type)");
      //check_equal_number(result[2].expected_size, 1, "transaction (size)");
      buffers_.transaction.clear();
   }

   void on_quit(resp3::type type) noexcept override
      { check_equal(buffers_.simple_string, {"OK"}, "quit"); }

   void on_publish(resp3::type type) noexcept override
   {
      check_equal((int)buffers_.number, 1, "publish (transaction)");
   }

   void on_ping(resp3::type type) noexcept override
   {
      check_equal(buffers_.simple_string, {"QUEUED"}, "ping");
      conn_->send([this](auto& req) { req.quit(); });
   }

   void on_multi(resp3::type type) noexcept override
   {
      check_equal(buffers_.simple_string, {"OK"}, "multi");
   }
};

void test_trans()
{
   net::io_context ioc;
   connection::config cfg{"127.0.0.1", "6379", 3, 10000};
   auto conn = std::make_shared<connection>(ioc.get_executor(), cfg);

   response_buffers bufs;
   trans_receiver recv{conn, bufs};
   conn->start(recv, bufs);
   ioc.run();
}

//-------------------------------------------------------------------

net::awaitable<void>
test_list(net::ip::tcp::resolver::results_type const& results)
{
   std::vector<int> list {1 ,2, 3, 4, 5, 6};

   pipeline p;
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
      detail::response_ignore res;
      co_await async_read(socket, buf, res);
   }

   {  // flushall
      resp3::simple_string buffer;
      detail::response_simple_string res{&buffer};
      co_await async_read(socket, buf, res);
      check_equal(buffer, {"OK"}, "flushall");
   }

   {  // rpush
      resp3::number buffer;
      detail::response_number res{&buffer};
      co_await async_read(socket, buf, res);
      check_equal(buffer, (long long int)6, "rpush");
   }

   {  // lrange
      resp3::array_int buffer;
      detail::response_basic_array<int> res{&buffer};
      co_await async_read(socket, buf, res);
      check_equal(buffer, list, "lrange-1");
   }

   {  // lrange
      resp3::array_int buffer;
      detail::response_basic_array<int> res{&buffer};
      co_await async_read(socket, buf, res);
      check_equal(buffer, std::vector<int>{3, 4, 5}, "lrange-2");
   }

   {  // ltrim
      resp3::simple_string buffer;
      detail::response_simple_string res{&buffer};
      co_await async_read(socket, buf, res);
      check_equal(buffer, {"OK"}, "ltrim");
   }

   {  // lpop. Why a blob string instead of a number?
      resp3::blob_string buffer;
      detail::response_blob_string res{&buffer};
      co_await async_read(socket, buf, res);
      check_equal(buffer, {"3"}, "lpop");
   }

   {  // quit
      resp3::simple_string buffer;
      detail::response_simple_string res{&buffer};
      co_await async_read(socket, buf, res);
      check_equal(buffer, {"OK"}, "quit");
   }
}

net::awaitable<void>
test_set(net::ip::tcp::resolver::results_type const& results)
{
   using namespace aedis::detail;
   auto ex = co_await this_coro::executor;

   // Tests whether the parser can handle payloads that contain the separator.
   std::string test_bulk1(10000, 'a');
   test_bulk1[30] = '\r';
   test_bulk1[31] = '\n';

   std::string test_bulk2 = "aaaaa";

   tcp_socket socket {ex};
   co_await async_connect(socket, results);

   pipeline p;
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
      response_ignore res;
      co_await async_read(socket, buf, res);
      co_await async_read(socket, buf, res);
   }

   {  // set
      resp3::simple_string buffer;
      response_simple_string res{&buffer};
      co_await async_read(socket, buf, res);
      check_equal(buffer, {"OK"}, "set1");
   }

   {  // get
      resp3::blob_string buffer;
      response_blob_string res{&buffer};
      co_await async_read(socket, buf, res);
      check_equal(buffer, test_bulk1, "get1");
   }

   {  // set
      resp3::simple_string buffer;
      response_simple_string res{&buffer};
      co_await async_read(socket, buf, res);
      check_equal(buffer, {"OK"}, "set1");
   }

   {  // get
      resp3::blob_string buffer;
      response_blob_string res{&buffer};
      co_await async_read(socket, buf, res);
      check_equal(buffer, test_bulk2, "get2");
   }

   {  // set
      resp3::simple_string buffer;
      response_simple_string res{&buffer};
      co_await async_read(socket, buf, res);
      check_equal(buffer, {"OK"}, "set3");
   }

   {  // get
      resp3::blob_string buffer;
      response_blob_string res{&buffer};
      co_await async_read(socket, buf, res);
      check_equal(buffer,  std::string {}, "get3");
   }

   {  // quit
      resp3::simple_string buffer;
      response_simple_string res{&buffer};
      co_await async_read(socket, buf, res);
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
   using namespace aedis::detail;
   {  // Small string
      std::string buf;
      std::string cmd {"+OK\r\n"};
      test_tcp_socket ts {cmd};
      resp3::simple_string buffer;
      response_simple_string res{&buffer};
      co_await async_read(ts, buf, res);
      check_equal(buffer, {"OK"}, "simple_string");
      //check_equal(res.attribute.value, {}, "simple_string (empty attribute)");
   }

   {  // empty
      std::string buf;
      std::string cmd {"+\r\n"};
      test_tcp_socket ts {cmd};
      resp3::simple_string buffer;
      response_simple_string res{&buffer};
      co_await async_read(ts, buf, res);
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
   //   response_simple_string res;
   //   co_await async_read(ts, buffer, res);
   //   check_equal(res.result, str, "simple_string (large)");
   //   //check_equal(res.attribute.value, {}, "simple_string (empty attribute)");
   //}
}

net::awaitable<void> test_number()
{
   using namespace aedis::detail;
   std::string buf;
   {  // int
      std::string cmd {":-3\r\n"};
      test_tcp_socket ts {cmd};
      resp3::number buffer;
      response_number res{&buffer};
      co_await async_read(ts, buf, res);
      check_equal(buffer, (long long int)-3, "number (int)");
   }

   {  // unsigned
      std::string cmd {":3\r\n"};
      test_tcp_socket ts {cmd};
      resp3::number buffer;
      response_number res{&buffer};
      co_await async_read(ts, buf, res);
      check_equal(buffer, (long long int)3, "number (unsigned)");
   }

   {  // std::size_t
      std::string cmd {":1111111\r\n"};
      test_tcp_socket ts {cmd};
      resp3::number buffer;
      response_number res{&buffer};
      co_await async_read(ts, buf, res);
      check_equal(buffer, (long long int)1111111, "number (std::size_t)");
   }
}

net::awaitable<void> test_array()
{
   using namespace aedis::detail;
   std::string buf;
   {  // Dynamic
      std::string cmd {"*3\r\n$3\r\none\r\n$3\r\ntwo\r\n$5\r\nthree\r\n"};
      test_tcp_socket ts {cmd};
      resp3::array buffer;
      response_array res{&buffer};
      co_await async_read(ts, buf, res);
      check_equal(buffer, {"one", "two", "three"}, "array (dynamic)");
   }

   {  // Static
      std::string cmd {"*3\r\n$3\r\none\r\n$3\r\ntwo\r\n$5\r\nthree\r\n"};
      test_tcp_socket ts {cmd};
      response_static_array<std::string, 3> res;
      co_await async_read(ts, buf, res);
      check_equal(res.result, {"one", "two", "three"}, "array (static)");
   }

   {  // Static int
      std::string cmd {"*3\r\n$1\r\n1\r\n$1\r\n2\r\n$1\r\n3\r\n"};
      test_tcp_socket ts {cmd};
      response_static_array<int, 3> res;
      co_await async_read(ts, buf, res);
      check_equal(res.result, {1, 2, 3}, "array (int)");
   }

   {
      std::string cmd {"*0\r\n"};
      test_tcp_socket ts {cmd};
      resp3::array buffer;
      response_array res{&buffer};
      co_await async_read(ts, buf, res);
      check_equal(buffer, {}, "array (empty)");
   }
}

net::awaitable<void> test_blob_string()
{
   using namespace aedis::detail;
   std::string buf;
   {
      std::string cmd {"$2\r\nhh\r\n"};
      test_tcp_socket ts {cmd};
      resp3::blob_string buffer;
      response_blob_string res{&buffer};
      co_await async_read(ts, buf, res);
      check_equal(buffer, {"hh"}, "blob_string");
   }

   {
      std::string cmd {"$26\r\nhhaa\aaaa\raaaaa\r\naaaaaaaaaa\r\n"};
      test_tcp_socket ts {cmd};
      resp3::blob_string buffer;
      response_blob_string res{&buffer};
      co_await async_read(ts, buf, res);
      check_equal(buffer, {"hhaa\aaaa\raaaaa\r\naaaaaaaaaa"}, "blob_string (with separator)");
   }

   {
      std::string cmd {"$0\r\n\r\n"};
      test_tcp_socket ts {cmd};
      resp3::blob_string buffer;
      response_blob_string res{&buffer};
      co_await async_read(ts, buf, res);
      check_equal(buffer, {}, "blob_string (size 0)");
   }
}

net::awaitable<void> test_simple_error()
{
   using namespace aedis::detail;
   std::string buf;
   {
      std::string cmd {"-Error\r\n"};
      test_tcp_socket ts {cmd};
      resp3::simple_error buffer;
      response_simple_error res{&buffer};
      co_await async_read(ts, buf, res);
      check_equal(buffer, {"Error"}, "simple_error (message)");
   }
}

net::awaitable<void> test_floating_point()
{
   using namespace aedis::detail;
   std::string buf;
   {
      std::string cmd {",1.23\r\n"};
      test_tcp_socket ts {cmd};
      resp3::doublean buffer;
      response_double res{&buffer};
      co_await async_read(ts, buf, res);
      check_equal(buffer, {"1.23"}, "double");
   }

   {
      std::string cmd {",inf\r\n"};
      test_tcp_socket ts {cmd};
      resp3::doublean buffer;
      response_double res{&buffer};
      co_await async_read(ts, buf, res);
      check_equal(buffer, {"inf"}, "double (inf)");
   }

   {
      std::string cmd {",-inf\r\n"};
      test_tcp_socket ts {cmd};
      resp3::doublean buffer;
      response_double res{&buffer};
      co_await async_read(ts, buf, res);
      check_equal(buffer, {"-inf"}, "double (-inf)");
   }

}

net::awaitable<void> test_boolean()
{
   using namespace aedis::detail;
   std::string buf;
   {
      std::string cmd {"#f\r\n"};
      test_tcp_socket ts {cmd};
      resp3::boolean buffer;
      response_bool res{&buffer};
      co_await async_read(ts, buf, res);
      check_equal(buffer, false, "bool (false)");
   }

   {
      std::string cmd {"#t\r\n"};
      test_tcp_socket ts {cmd};
      resp3::boolean buffer;
      response_bool res{&buffer};
      co_await async_read(ts, buf, res);
      check_equal(buffer, true, "bool (true)");
   }
}

net::awaitable<void> test_blob_error()
{
   using namespace aedis::detail;
   std::string buf;
   {
      std::string cmd {"!21\r\nSYNTAX invalid syntax\r\n"};
      test_tcp_socket ts {cmd};
      resp3::blob_error buffer;
      response_blob_error res{&buffer};
      co_await async_read(ts, buf, res);
      check_equal(buffer, {"SYNTAX invalid syntax"}, "blob_error (message)");
   }

   {
      std::string cmd {"!0\r\n\r\n"};
      test_tcp_socket ts {cmd};
      resp3::blob_error buffer;
      response_blob_error res{&buffer};
      co_await async_read(ts, buf, res);
      check_equal(buffer, {}, "blob_error (empty message)");
   }
}

net::awaitable<void> test_verbatim_string()
{
   using namespace aedis::detail;
   std::string buf;
   {
      std::string cmd {"=15\r\ntxt:Some string\r\n"};
      test_tcp_socket ts {cmd};
      resp3::verbatim_string buffer;
      response_verbatim_string res{&buffer};
      co_await async_read(ts, buf, res);
      check_equal(buffer, {"txt:Some string"}, "verbatim_string");
   }

   {
      std::string cmd {"=0\r\n\r\n"};
      test_tcp_socket ts {cmd};
      resp3::verbatim_string buffer;
      response_verbatim_string res{&buffer};
      co_await async_read(ts, buf, res);
      check_equal(buffer, {}, "verbatim_string (empty)");
   }
}

net::awaitable<void> test_set2()
{
   using namespace aedis::detail;
   std::string buf;
   {
      std::string cmd {"~5\r\n+orange\r\n+apple\r\n+one\r\n+two\r\n+three\r\n"};
      test_tcp_socket ts {cmd};
      resp3::set buffer;
      response_set res{&buffer};
      co_await async_read(ts, buf, res);
      check_equal(buffer, {"orange", "apple", "one", "two", "three"}, "set");
   }

   {
      std::string cmd {"~5\r\n+orange\r\n+apple\r\n+one\r\n+two\r\n+three\r\n"};
      test_tcp_socket ts {cmd};
      resp3::set buffer;
      response_set res{&buffer};
      co_await async_read(ts, buf, res);
      check_equal(buffer, {"orange", "apple", "one", "two", "three"}, "set (flat)");
   }

   {
      std::string cmd {"~0\r\n"};
      test_tcp_socket ts {cmd};
      resp3::set buffer;
      response_set res{&buffer};
      co_await async_read(ts, buf, res);
      check_equal(buffer, {}, "set (empty)");
   }
}

net::awaitable<void> test_map()
{
   using namespace aedis::detail;
   std::string buf;
   {
      std::string cmd {"%7\r\n$6\r\nserver\r\n$5\r\nredis\r\n$7\r\nversion\r\n$5\r\n6.0.9\r\n$5\r\nproto\r\n:3\r\n$2\r\nid\r\n:203\r\n$4\r\nmode\r\n$10\r\nstandalone\r\n$4\r\nrole\r\n$6\r\nmaster\r\n$7\r\nmodules\r\n*0\r\n"};
      test_tcp_socket ts {cmd};
      resp3::map buffer;
      response_map res{&buffer};
      co_await async_read(ts, buf, res);
      check_equal(buffer, {"server", "redis", "version", "6.0.9", "proto", "3", "id", "203", "mode", "standalone", "role", "master", "modules"}, "map (flat)");
   }

   {
      std::string cmd {"%0\r\n"};
      test_tcp_socket ts {cmd};
      resp3::map buffer;
      response_map res{&buffer};
      co_await async_read(ts, buf, res);
      check_equal(buffer, {}, "map (flat - empty)");
   }
}

net::awaitable<void> test_streamed_string()
{
   using namespace aedis::detail;
   std::string buf;
   {
      std::string cmd {"$?\r\n;4\r\nHell\r\n;5\r\no wor\r\n;1\r\nd\r\n;0\r\n"};
      test_tcp_socket ts {cmd};
      resp3::streamed_string_part buffer;
      response_streamed_string_part res{&buffer};
      co_await async_read(ts, buf, res);
      check_equal(buffer, {"Hello word"}, "streamed string");
   }

   {
      std::string cmd {"$?\r\n;0\r\n"};
      test_tcp_socket ts {cmd};
      resp3::array buffer;
      response_array res{&buffer};
      co_await async_read(ts, buf, res);
      check_equal(buffer, {}, "streamed string (empty)");
   }
}

net::awaitable<void> offline()
{
   std::string buf;
   //{
   //   std::string cmd {"|1\r\n+key-popularity\r\n%2\r\n$1\r\na\r\n,0.1923\r\n$1\r\nb\r\n,0.0012\r\n"};
   //   test_tcp_socket ts {cmd};
   //   response_array res;
   //   co_await async_read(ts, buf, res);
   //   check_equal(res.result, {"key-popularity", "a", "0.1923", "b", "0.0012"}, "attribute");
   //}

   //{
   //   std::string cmd {">4\r\n+pubsub\r\n+message\r\n+foo\r\n+bar\r\n"};
   //   test_tcp_socket ts {cmd};
   //   response_array res;
   //   co_await async_read(ts, buf, res);
   //   check_equal(res.result, {"pubsub", "message", "foo", "bar"}, "push type");
   //}

   //{
   //   std::string cmd {">0\r\n"};
   //   test_tcp_socket ts {cmd};
   //   response_array res;
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

   co_spawn(ioc, test_list(res), net::detached);
   co_spawn(ioc, test_set(res), net::detached);
   co_spawn(ioc, test_general(res), net::detached);
   ioc.run();
   test_trans();
}

