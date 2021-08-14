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

class test_receiver : public receiver_base {
private:
   std::shared_ptr<connection> conn_;

   std::vector<int> list_ {1 ,2, 3, 4, 5, 6};
   std::string set_ {"aaa"};

public:
   test_receiver(std::shared_ptr<connection> conn) : conn_{conn} { }

   void on_hello(array_type& v) noexcept override
   {
      auto f = [this](auto& req)
      {
	 req.flushall();
	 req.rpush("a", list_);
	 req.llen("a");
	 req.lrange("a");
	 req.ltrim("a", 2, -2);
	 req.lpop("a");
	 //req.lpop("a", 2); // Not working?
	 req.set("b", {set_});
	 req.get("b");
	 req.append("b", "b");
	 req.del("b");
	 req.subscribe("channel");
	 req.publish("channel", "message");
	 req.incr("c");

	 std::map<std::string, std::string> m1 =
	 { {"field1", "value1"}
	 , {"field2", "value2"}};

	 req.hset("d", m1);
	 req.hget("d", "field2");
	 req.hgetall("d");
	 req.hdel("d", {"field1", "field2"});
	 req.hincrby("e", "some-field", 10);

	 req.zadd("f", 1, "Marcelo");
	 req.zrange("f");
	 req.zrangebyscore("f", 1, 1);
	 req.zremrangebyscore("f", "-inf", "+inf");

	 req.sadd("g", std::vector<int>{1, 2, 3});
	 req.smembers("g");

	 req.quit();
      };

      conn_->send(f);
   }

   void on_simple_error(commands cmd, simple_error_type& v) noexcept override
   {
      std::cout << v << std::endl;
   }

   void on_push(array_type& v) noexcept override
   {
      // TODO: Check the responses below.
      // {"subscribe", "channel", "1"}
      // {"message", "channel", "message"}
      check_equal(1, 1, "push (receiver)");
   }

   // Blob string.
   void on_get(blob_string_type& s) noexcept override
      { check_equal(s, set_, "get (receiver)"); }

   void on_hget(blob_string_type& s) noexcept override
      { check_equal(s, std::string{"value2"}, "hget (receiver)"); }

   void on_lpop(blob_string_type& s) noexcept override
      { check_equal(s, {"3"}, "lpop (receiver)"); }

   // Array
   void on_lrange(array_type& v) noexcept override
      { check_equal(v, {"1", "2", "3", "4", "5", "6"}, "lrange (receiver)"); }

   void on_lpop(array_type& s) noexcept override
      { check_equal(s, {"4", "5"}, "lpop(count) (receiver)"); }

   void on_hgetall(array_type& s) noexcept override
      { check_equal(s, {"field1", "value1", "field2", "value2"}, "hgetall (receiver)"); }

   void on_hvals(array_type& s) noexcept override
      { check_equal(s, {"value1", "value2"}, "hvals (receiver)"); }

   void on_zrange(array_type& s) noexcept override
      { check_equal(s, {"Marcelo"}, "zrange (receiver)"); }

   void on_zrangebyscore(array_type& s) noexcept override
      { check_equal(s, {"Marcelo"}, "zrangebyscore (receiver)"); }

   void on_smembers(set_type& s) noexcept override
      { check_equal(s, {"1", "2", "3"}, "smembers (receiver)"); }

   // Simple string
   void on_set(simple_string_type& s) noexcept override
      { check_equal(s, {"OK"}, "set (receiver)"); }

   void on_quit(simple_string_type& s) noexcept override
      { check_equal(s, {"OK"}, "quit (receiver)"); }

   void on_flushall(simple_string_type& s) noexcept override
      { check_equal(s, {"OK"}, "flushall (receiver)"); }

   void on_ltrim(simple_string_type& s) noexcept override
      { check_equal(s, {"OK"}, "ltrim (receiver)"); }

   // Number
   void on_append(number_type n) noexcept override
      { check_equal((int)n, 4, "append (receiver)"); }

   void on_hset(number_type n) noexcept override
      { check_equal((int)n, 2, "hset (receiver)"); }

   void on_rpush(number_type n) noexcept override
      { check_equal(n, (number_type)std::size(list_), "rpush (receiver)"); }

   void on_del(number_type n) noexcept override
      { check_equal((int)n, 1, "del (receiver)"); }

   void on_llen(number_type n) noexcept override
      { check_equal((int)n, 6, "llen (receiver)"); }

   void on_incr(number_type n) noexcept override
      { check_equal((int)n, 1, "incr (receiver)"); }

   void on_publish(number_type n) noexcept override
      { check_equal((int)n, 1, "publish (receiver)"); }

   void on_hincrby(number_type n) noexcept override
      { check_equal((int)n, 10, "hincrby (receiver)"); }

   void on_zadd(number_type n) noexcept override
      { check_equal((int)n, 1, "zadd (receiver)"); }

   void on_zremrangebyscore(number_type s) noexcept override
      { check_equal((int)s, 1, "zremrangebyscore (receiver)"); }

   void on_sadd(number_type n) noexcept override
      { check_equal((int)n, 3, "sadd (receiver)"); }

   void on_hdel(number_type n) noexcept override
      { check_equal((int)n, 2, "hdel (receiver)"); }

};

void test_receiver_1()
{
   net::io_context ioc;
   auto conn = std::make_shared<connection>(ioc.get_executor());
   test_receiver recv{conn};
   conn->start(recv);
   ioc.run();
}

//-------------------------------------------------------------------

class ping_receiver : public receiver_base {
private:
   std::shared_ptr<connection> conn_;

public:
   ping_receiver(std::shared_ptr<connection> conn) : conn_{conn} { }

   void on_hello(array_type& v) noexcept override
      { conn_->send([this](auto& req) { req.ping(); }); }

   void on_ping(simple_string_type& s) noexcept override
   {
      check_equal(s, {"PONG"}, "ping");
      conn_->send([this](auto& req) { req.quit(); });
   }

   void on_quit(simple_string_type& s) noexcept override
      { check_equal(s, {"OK"}, "quit"); }
};

void test_ping()
{
   net::io_context ioc;
   auto conn = std::make_shared<connection>(ioc.get_executor());
   ping_receiver recv{conn};
   conn->start(recv);
   ioc.run();
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
   req.ping();
   req.publish("some-channel", "message2");
};

class trans_receiver : public receiver_base {
private:
   int counter_ = 0;
   std::shared_ptr<connection> conn_;

public:
   trans_receiver(std::shared_ptr<connection> conn) : conn_{conn} { }

   void on_hello(array_type& v) noexcept override
   {
      auto f = [this](auto& req)
      {
	 req.flushall();
	 trans_filler(req);
      };

      for (auto i = 0; i < 20; ++i) {
	 conn_->send(f);
      }
   }

   void on_push(array_type& v) noexcept override
   {
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
   }

   void on_flushall(simple_string_type& s) noexcept override
      { check_equal(s, {"OK"}, "flushall (transaction)"); }

   void
   on_transaction(std::vector<transaction_element>& result) noexcept override
   {
      check_equal(result[0].command, commands::ping, "transaction ping (command)");
      check_equal(result[0].depth, 1, "transaction (depth)");
      check_equal(result[0].type, types::simple_string, "transaction (type)");
      check_equal(result[0].expected_size, 1, "transaction (size)");

      check_equal(result[1].command, commands::ping, "transaction incr (command)");
      check_equal(result[1].depth, 1, "transaction (depth)");
      check_equal(result[1].type, types::simple_string, "transaction (typ)e");
      check_equal(result[1].expected_size, 1, "transaction (size)");

      // See note above
      //check_equal(result[2].command, commands::publish, "transaction publish (command)");
      //check_equal_number(result[2].depth, 1, "transaction (depth)");
      //check_equal_number(result[2].type, types::number, "transaction (type)");
      //check_equal_number(result[2].expected_size, 1, "transaction (size)");
      result.clear();
   }

   void on_quit(simple_string_type& s) noexcept override
      { check_equal(s, {"OK"}, "quit"); }

   void on_publish(number_type n) noexcept override
   {
      check_equal((int)n, 1, "publish (transaction)");
   }

   void on_ping(simple_string_type& s) noexcept override
   {
      check_equal(s, {"PONG"}, "ping");
      conn_->send([this](auto& req) { req.quit(); });
   }
};

void test_trans()
{
   net::io_context ioc;
   connection::config cfg{"127.0.0.1", "6379", 3, 10000};
   
   auto conn = std::make_shared<connection>(ioc.get_executor(), cfg);
   trans_receiver recv{conn};
   conn->start(recv);
   ioc.run();
}

//-------------------------------------------------------------------

net::awaitable<void>
test_list(net::ip::tcp::resolver::results_type const& results)
{
   std::vector<int> list {1 ,2, 3, 4, 5, 6};

   request p;
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
   std::string buffer;

   {  // hello
      detail::response_ignore res;
      co_await async_read(socket, buffer, res);
   }

   {  // flushall
      detail::response_simple_string res;
      co_await async_read(socket, buffer, res);
      check_equal(res.result, {"OK"}, "flushall");
   }

   {  // rpush
      detail::response_number res;
      co_await async_read(socket, buffer, res);
      check_equal(res.result, (long long int)6, "rpush");
   }

   {  // lrange
      detail::response_basic_array<int> res;
      co_await async_read(socket, buffer, res);
      check_equal(res.result, list, "lrange-1");
   }

   {  // lrange
      detail::response_basic_array<int> res;
      co_await async_read(socket, buffer, res);
      check_equal(res.result, std::vector<int>{3, 4, 5}, "lrange-2");
   }

   {  // ltrim
      detail::response_simple_string res;
      co_await async_read(socket, buffer, res);
      check_equal(res.result, {"OK"}, "ltrim");
   }

   {  // lpop. Why a blob string instead of a number?
      detail::response_blob_string res;
      co_await async_read(socket, buffer, res);
      check_equal(res.result, {"3"}, "lpop");
   }

   {  // quit
      detail::response_simple_string res;
      co_await async_read(socket, buffer, res);
      check_equal(res.result, {"OK"}, "quit");
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

   request p;
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

   std::string buffer;
   {  // hello, flushall
      response_ignore res;
      co_await async_read(socket, buffer, res);
      co_await async_read(socket, buffer, res);
   }

   {  // set
      response_simple_string res;
      co_await async_read(socket, buffer, res);
      check_equal(res.result, {"OK"}, "set1");
   }

   {  // get
      response_blob_string res;
      co_await async_read(socket, buffer, res);
      check_equal(res.result, test_bulk1, "get1");
   }

   {  // set
      response_simple_string res;
      co_await async_read(socket, buffer, res);
      check_equal(res.result, {"OK"}, "set1");
   }

   {  // get
      response_blob_string res;
      co_await async_read(socket, buffer, res);
      check_equal(res.result, test_bulk2, "get2");
   }

   {  // set
      response_simple_string res;
      co_await async_read(socket, buffer, res);
      check_equal(res.result, {"OK"}, "set3");
   }

   {  // get
      response_blob_string res;
      co_await async_read(socket, buffer, res);
      check_equal(res.result,  std::string {}, "get3");
   }

   {  // quit
      response_simple_string res;
      co_await async_read(socket, buffer, res);
      check_equal(res.result, {"OK"}, "quit");
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
      std::string buffer;
      std::string cmd {"+OK\r\n"};
      test_tcp_socket ts {cmd};
      response_simple_string res;
      co_await async_read(ts, buffer, res);
      check_equal(res.result, {"OK"}, "simple_string");
      //check_equal(res.attribute.value, {}, "simple_string (empty attribute)");
   }

   {  // empty
      std::string buffer;
      std::string cmd {"+\r\n"};
      test_tcp_socket ts {cmd};
      response_simple_string res;
      co_await async_read(ts, buffer, res);
      check_equal(res.result, {}, "simple_string (empty)");
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
   std::string buffer;
   {  // int
      std::string cmd {":-3\r\n"};
      test_tcp_socket ts {cmd};
      response_number res;
      co_await async_read(ts, buffer, res);
      check_equal(res.result, (long long int)-3, "number (int)");
   }

   {  // unsigned
      std::string cmd {":3\r\n"};
      test_tcp_socket ts {cmd};
      response_number res;
      co_await async_read(ts, buffer, res);
      check_equal(res.result, (long long int)3, "number (unsigned)");
   }

   {  // std::size_t
      std::string cmd {":1111111\r\n"};
      test_tcp_socket ts {cmd};
      response_number res;
      co_await async_read(ts, buffer, res);
      check_equal(res.result, (long long int)1111111, "number (std::size_t)");
   }
}

net::awaitable<void> test_array()
{
   using namespace aedis::detail;
   std::string buffer;
   {  // Dynamic
      std::string cmd {"*3\r\n$3\r\none\r\n$3\r\ntwo\r\n$5\r\nthree\r\n"};
      test_tcp_socket ts {cmd};
      response_array res;
      co_await async_read(ts, buffer, res);
      check_equal(res.result, {"one", "two", "three"}, "array (dynamic)");
   }

   {  // Static
      std::string cmd {"*3\r\n$3\r\none\r\n$3\r\ntwo\r\n$5\r\nthree\r\n"};
      test_tcp_socket ts {cmd};
      response_static_array<std::string, 3> res;
      co_await async_read(ts, buffer, res);
      check_equal(res.result, {"one", "two", "three"}, "array (static)");
   }

   {  // Static int
      std::string cmd {"*3\r\n$1\r\n1\r\n$1\r\n2\r\n$1\r\n3\r\n"};
      test_tcp_socket ts {cmd};
      response_static_array<int, 3> res;
      co_await async_read(ts, buffer, res);
      check_equal(res.result, {1, 2, 3}, "array (int)");
   }

   {
      std::string cmd {"*0\r\n"};
      test_tcp_socket ts {cmd};
      response_array res;
      co_await async_read(ts, buffer, res);
      check_equal(res.result, {}, "array (empty)");
   }
}

net::awaitable<void> test_blob_string()
{
   using namespace aedis::detail;
   std::string buffer;
   {
      std::string cmd {"$2\r\nhh\r\n"};
      test_tcp_socket ts {cmd};
      response_blob_string res;
      co_await async_read(ts, buffer, res);
      check_equal(res.result, {"hh"}, "blob_string");
   }

   {
      std::string cmd {"$26\r\nhhaa\aaaa\raaaaa\r\naaaaaaaaaa\r\n"};
      test_tcp_socket ts {cmd};
      response_blob_string res;
      co_await async_read(ts, buffer, res);
      check_equal(res.result, {"hhaa\aaaa\raaaaa\r\naaaaaaaaaa"}, "blob_string (with separator)");
   }

   {
      std::string cmd {"$0\r\n\r\n"};
      test_tcp_socket ts {cmd};
      response_blob_string res;
      co_await async_read(ts, buffer, res);
      check_equal(res.result, {}, "blob_string (size 0)");
   }
}

net::awaitable<void> test_simple_error()
{
   using namespace aedis::detail;
   std::string buffer;
   {
      std::string cmd {"-Error\r\n"};
      test_tcp_socket ts {cmd};
      response_simple_error res;
      co_await async_read(ts, buffer, res);
      check_equal(res.result, {"Error"}, "simple_error (message)");
   }
}

net::awaitable<void> test_floating_point()
{
   using namespace aedis::detail;
   std::string buffer;
   {
      std::string cmd {",1.23\r\n"};
      test_tcp_socket ts {cmd};
      response_double res;
      co_await async_read(ts, buffer, res);
      check_equal(res.result, {"1.23"}, "double");
   }

   {
      std::string cmd {",inf\r\n"};
      test_tcp_socket ts {cmd};
      response_double res;
      co_await async_read(ts, buffer, res);
      check_equal(res.result, {"inf"}, "double (inf)");
   }

   {
      std::string cmd {",-inf\r\n"};
      test_tcp_socket ts {cmd};
      response_double res;
      co_await async_read(ts, buffer, res);
      check_equal(res.result, {"-inf"}, "double (-inf)");
   }

}

net::awaitable<void> test_boolean()
{
   using namespace aedis::detail;
   std::string buffer;
   {
      std::string cmd {"#f\r\n"};
      test_tcp_socket ts {cmd};
      response_bool res;
      co_await async_read(ts, buffer, res);
      check_equal(res.result, false, "bool (false)");
   }

   {
      std::string cmd {"#t\r\n"};
      test_tcp_socket ts {cmd};
      response_bool res;
      co_await async_read(ts, buffer, res);
      check_equal(res.result, true, "bool (true)");
   }
}

net::awaitable<void> test_blob_error()
{
   using namespace aedis::detail;
   std::string buffer;
   {
      std::string cmd {"!21\r\nSYNTAX invalid syntax\r\n"};
      test_tcp_socket ts {cmd};
      response_blob_error res;
      co_await async_read(ts, buffer, res);
      check_equal(res.result, {"SYNTAX invalid syntax"}, "blob_error (message)");
   }

   {
      std::string cmd {"!0\r\n\r\n"};
      test_tcp_socket ts {cmd};
      response_blob_error res;
      co_await async_read(ts, buffer, res);
      check_equal(res.result, {}, "blob_error (empty message)");
   }
}

net::awaitable<void> test_verbatim_string()
{
   using namespace aedis::detail;
   std::string buffer;
   {
      std::string cmd {"=15\r\ntxt:Some string\r\n"};
      test_tcp_socket ts {cmd};
      response_verbatim_string res;
      co_await async_read(ts, buffer, res);
      check_equal(res.result, {"txt:Some string"}, "verbatim_string");
   }

   {
      std::string cmd {"=0\r\n\r\n"};
      test_tcp_socket ts {cmd};
      response_verbatim_string res;
      co_await async_read(ts, buffer, res);
      check_equal(res.result, {}, "verbatim_string (empty)");
   }
}

net::awaitable<void> test_set2()
{
   using namespace aedis::detail;
   std::string buffer;
   {
      std::string cmd {"~5\r\n+orange\r\n+apple\r\n+one\r\n+two\r\n+three\r\n"};
      test_tcp_socket ts {cmd};
      response_set res;
      co_await async_read(ts, buffer, res);
      check_equal(res.result, {"orange", "apple", "one", "two", "three"}, "set");
   }

   {
      std::string cmd {"~5\r\n+orange\r\n+apple\r\n+one\r\n+two\r\n+three\r\n"};
      test_tcp_socket ts {cmd};
      response_set res;
      co_await async_read(ts, buffer, res);
      check_equal(res.result, {"orange", "apple", "one", "two", "three"}, "set (flat)");
   }

   {
      std::string cmd {"~0\r\n"};
      test_tcp_socket ts {cmd};
      response_set res;
      co_await async_read(ts, buffer, res);
      check_equal(res.result, {}, "set (empty)");
   }
}

net::awaitable<void> test_map()
{
   using namespace aedis::detail;
   std::string buffer;
   {
      std::string cmd {"%7\r\n$6\r\nserver\r\n$5\r\nredis\r\n$7\r\nversion\r\n$5\r\n6.0.9\r\n$5\r\nproto\r\n:3\r\n$2\r\nid\r\n:203\r\n$4\r\nmode\r\n$10\r\nstandalone\r\n$4\r\nrole\r\n$6\r\nmaster\r\n$7\r\nmodules\r\n*0\r\n"};
      test_tcp_socket ts {cmd};
      response_map res;
      co_await async_read(ts, buffer, res);
      check_equal(res.result, {"server", "redis", "version", "6.0.9", "proto", "3", "id", "203", "mode", "standalone", "role", "master", "modules"}, "map (flat)");
   }

   {
      std::string cmd {"%0\r\n"};
      test_tcp_socket ts {cmd};
      response_map res;
      co_await async_read(ts, buffer, res);
      check_equal(res.result, {}, "map (flat - empty)");
   }
}

net::awaitable<void> test_streamed_string()
{
   using namespace aedis::detail;
   std::string buffer;
   {
      std::string cmd {"$?\r\n;4\r\nHell\r\n;5\r\no wor\r\n;1\r\nd\r\n;0\r\n"};
      test_tcp_socket ts {cmd};
      response_streamed_string_part res;
      co_await async_read(ts, buffer, res);
      check_equal(res.result, {"Hello word"}, "streamed string");
   }

   {
      std::string cmd {"$?\r\n;0\r\n"};
      test_tcp_socket ts {cmd};
      response_array res;
      co_await async_read(ts, buffer, res);
      check_equal(res.result, {}, "streamed string (empty)");
   }
}

net::awaitable<void> offline()
{
   std::string buffer;
   //{
   //   std::string cmd {"|1\r\n+key-popularity\r\n%2\r\n$1\r\na\r\n,0.1923\r\n$1\r\nb\r\n,0.0012\r\n"};
   //   test_tcp_socket ts {cmd};
   //   response_array res;
   //   co_await async_read(ts, buffer, res);
   //   check_equal(res.result, {"key-popularity", "a", "0.1923", "b", "0.0012"}, "attribute");
   //}

   //{
   //   std::string cmd {">4\r\n+pubsub\r\n+message\r\n+foo\r\n+bar\r\n"};
   //   test_tcp_socket ts {cmd};
   //   response_array res;
   //   co_await async_read(ts, buffer, res);
   //   check_equal(res.result, {"pubsub", "message", "foo", "bar"}, "push type");
   //}

   //{
   //   std::string cmd {">0\r\n"};
   //   test_tcp_socket ts {cmd};
   //   response_array res;
   //   co_await async_read(ts, buffer, res);
   //   check_equal(res.result, {}, "push type (empty)");
   //}
}

int main(int argc, char* argv[])
{
   net::io_context ioc {1};
   tcp::resolver resv(ioc);
   auto const results = resv.resolve("127.0.0.1", "6379");

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

   co_spawn(ioc, test_list(results), net::detached);
   co_spawn(ioc, test_set(results), net::detached);
   ioc.run();

   test_receiver_1();

   test_ping();
   test_trans();
}

