/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>
#include <map>

#include <boost/asio.hpp>
#include <boost/system/errc.hpp>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

#include "check.hpp"

namespace net = boost::asio;
namespace resp3 = aedis::resp3;

using aedis::resp3::node;
using aedis::redis::command;
using aedis::generic::make_serializer;
using aedis::adapter::adapt;
using aedis::adapter::adapter_t;
using node_type = aedis::resp3::node<std::string>;
using tcp = net::ip::tcp;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<tcp::socket>;
using client_type = aedis::generic::client<net::ip::tcp::socket, command>;

auto print_read = [](auto cmd, auto n)
{
   std::cout << cmd << ": " << n << std::endl;
};

void test_resolve_error()
{
   auto f = [](auto ec)
   {
      expect_error(ec, net::error::netdb_errors::host_not_found);
   };

   net::io_context ioc;
   client_type db(ioc.get_executor());
   db.async_run("Atibaia", "6379", f);
   ioc.run();
}

void test_connect_error()
{
   auto f = [](auto ec)
   {
      expect_error(ec, net::error::basic_errors::connection_refused);
   };

   net::io_context ioc;
   client_type db(ioc.get_executor());
   db.async_run("127.0.0.1", "1", f);
   ioc.run();
}

struct receiver1 {
public:
   receiver1(client_type& db) : db_{&db} {}

   void on_read(command cmd, std::size_t)
   {
      // quit will be sent more than once. It doesn't matter.
      db_->send(command::quit);
   }

private:
   client_type* db_;
};

// Test if a hello is automatically sent.
void test_hello()
{
   auto f = [](auto ec)
   {
      expect_error(ec, net::error::misc_errors::eof);
   };

   net::io_context ioc;
   client_type db(ioc.get_executor());
   receiver1 recv{db};
   db.set_read_handler([&recv](command cmd, std::size_t n){recv.on_read(cmd, n);});
   db.async_run("127.0.0.1", "6379", f);
   ioc.run();
}

struct receiver2 {
public:
   receiver2(client_type& db) : db_{&db} {}

   void on_write(std::size_t)
   {
      // Notice this causes a loop, but since quit stops the
      // connection it is not a problem.
      db_->send(command::quit);
   }

private:
   client_type* db_;
};

// Test if a hello is automatically sent but this time, uses on_write
// to send the quit command. Notice quit will be sent twice.
void test_hello2()
{
   auto f = [](auto ec)
   {
      expect_error(ec, net::error::misc_errors::eof);
   };

   net::io_context ioc;
   client_type db(ioc.get_executor());
   receiver2 recv{db};
   //db.set_read_handler(print_read);
   db.set_write_handler([&recv](std::size_t n){recv.on_write(n);});
   db.async_run("127.0.0.1", "6379", f);
   ioc.run();
}

struct receiver3 {
public:
   receiver3(client_type& db) : db_{&db} {}

   void on_write(std::size_t)
   {
      // Notice this causes a loop.
      db_->send(command::subscribe, "channel");
   }

   void on_push(std::size_t)
   {
      db_->send(command::quit);
   }

private:
   client_type* db_;
};

void test_push()
{
   auto f = [](auto ec)
   {
      expect_error(ec, net::error::misc_errors::eof);
   };

   net::io_context ioc;
   client_type db(ioc.get_executor());
   receiver3 recv{db};
   db.set_write_handler([&recv](std::size_t n){recv.on_write(n);});
   db.set_push_handler([&recv](std::size_t n){recv.on_push(n);});
   db.async_run("127.0.0.1", "6379", f);
   ioc.run();
}

struct receiver4 {
public:
   receiver4(client_type& db) : db_{&db} {}

   void on_read()
   {
      // Notice this causes a loop.
      db_->send(command::subscribe, "channel");
   }

   void on_push()
   {
      db_->send(command::quit);
   }

private:
   client_type* db_;
};

void test_push2()
{
   auto f = [](auto ec)
   {
      expect_error(ec, net::error::misc_errors::eof);
   };

   net::io_context ioc;
   client_type db(ioc.get_executor());
   receiver4 recv{db};
   db.set_read_handler([&recv](auto, auto){recv.on_read();});
   db.set_push_handler([&recv](auto){recv.on_push();});
   db.async_run("127.0.0.1", "6379", f);
   ioc.run();
}

#include <boost/asio/yield.hpp>

struct receiver5 {
public:
   int counter = 0;

   receiver5(client_type& db)
   : db_{&db}
   , adapter_{adapt(counter)}
   {}

   void on_write()
   {
      if (counter == 0) {
         // Avoid problems with previous runs.
         db_->send(command::del, "receiver5-key");
         db_->send(command::incr, "receiver5-key");
         db_->send(command::quit);
      }

      if (counter == 1) {
         db_->send(command::incr, "receiver5-key");
         db_->send(command::quit);
      }
   }

   void on_resp3(command cmd, node<boost::string_view> const& nd, boost::system::error_code& ec)
   {
      if (cmd == command::incr)
         adapter_(nd, ec);
   }

private:
   client_type* db_;
   adapter_t<int> adapter_;
};

struct reconnect {
   client_type db;
   receiver5 recv;
   boost::asio::steady_timer timer;
   net::coroutine coro;

   reconnect(net::any_io_executor ex)
   : db{ex}
   , recv{db}
   , timer{ex}
   {
      db.set_write_handler([this](auto){recv.on_write();});
      db.set_resp3_handler([this](auto a, auto b, auto c){recv.on_resp3(a, b, c);});
   }

   void on_event(boost::system::error_code ec)
   {
      reenter (coro) for (;;) {
         yield db.async_run("127.0.0.1", "6379",  [this](auto ec){ on_event(ec);});
         expect_error(ec, net::error::misc_errors::eof);
         expect_eq(recv.counter, 1, "Reconnect counter 1.");
         yield db.async_run("127.0.0.1", "6379",  [this](auto ec){ on_event(ec);});
         expect_error(ec, net::error::misc_errors::eof);
         expect_eq(recv.counter, 2, "Reconnect counter 2.");
         yield db.async_run("127.0.0.1", "6379",  [this](auto ec){ on_event(ec);});
         expect_error(ec, net::error::misc_errors::eof);
         expect_eq(recv.counter, 3, "Reconnect counter 3.");
         return;
      }
   }
};

#include <boost/asio/unyield.hpp>

void test_reconnect()
{
   net::io_context ioc;
   reconnect rec{ioc.get_executor()};
   rec.on_event({});
   ioc.run();
}

// TODO: test_reconnect2() using on_read instead of on_write.

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
   test_resolve_error();
   test_connect_error();
   test_hello();
   test_hello2();
   test_push();
   test_push2();
   test_reconnect();

   //net::io_context ioc {1};
   //tcp::resolver resv(ioc);
   //auto const res = resv.resolve("127.0.0.1", "6379");

   //co_spawn(ioc, test_general(res), net::detached);
   //ioc.run();
}

