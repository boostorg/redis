/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <iostream>
#include <map>

#include <boost/asio.hpp>
#include <boost/system/errc.hpp>
#include <boost/asio/experimental/as_tuple.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

#include "check.hpp"

//std::cout << "aaaa " << ec.message() << " " << cmd << " " << n << std::endl;

namespace net = boost::asio;
namespace resp3 = aedis::resp3;

using aedis::adapter::adapt;
using aedis::adapter::adapter_t;
using aedis::generic::make_serializer;
using aedis::redis::command;
using aedis::resp3::node;
using client_type = aedis::generic::client<net::ip::tcp::socket, command>;
using error_code = boost::system::error_code;
using namespace net::experimental::awaitable_operators;
using net::experimental::as_tuple;
using node_type = aedis::resp3::node<boost::string_view>;
using tcp = net::ip::tcp;
using boost::system::error_code;

auto print_read = [](auto cmd, auto n)
{
   std::cout << cmd << ": " << n << std::endl;
};

//----------------------------------------------------------------

void test_resolve_error()
{
   auto f = [](auto ec)
   {
      expect_error(ec, net::error::netdb_errors::host_not_found);
   };

   net::io_context ioc;
   client_type::config cfg;
   cfg.host = "Atibaia";
   client_type db(ioc.get_executor(), cfg);
   db.async_run(f);
   ioc.run();
}

//----------------------------------------------------------------

void test_connect_error()
{
   auto f = [](auto ec)
   {
      expect_error(ec, net::error::basic_errors::connection_refused);
   };

   net::io_context ioc;
   client_type::config cfg;
   cfg.port = "1";
   client_type db(ioc.get_executor(), cfg);
   db.async_run(f);
   ioc.run();
}

//----------------------------------------------------------------

net::awaitable<void> reader1(std::shared_ptr<client_type> db)
{
   {  // Read the hello.
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_error(ec, error_code{});
      expect_eq(cmd, command::hello);
   }

   db->send(command::quit);

   {  // Read the quit.
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_error(ec, error_code{});
      expect_eq(cmd, command::quit);
   }
}

net::awaitable<void> run1(std::shared_ptr<client_type> db)
{
   auto [ec] = co_await db->async_run(as_tuple(net::use_awaitable));
   expect_error(ec, net::error::misc_errors::eof);
}

// Test if a hello is sent automatically.
void test_hello()
{
   net::io_context ioc;
   auto db = std::make_shared<client_type>(ioc.get_executor());
   net::co_spawn(ioc.get_executor(), run1(db), net::detached);
   net::co_spawn(ioc.get_executor(), reader1(db), net::detached);
   ioc.run();
}

//----------------------------------------------------------------

net::awaitable<void> reader2(std::shared_ptr<client_type> db)
{
   {  // Read the hello.
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_error(ec, error_code{});
      expect_eq(cmd, command::hello);
   }

   {  // Read the quit.
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_error(ec, error_code{});
      expect_eq(cmd, command::quit);
   }
}

// Test if a hello is automatically sent but this time, uses on_write
// to send the quit command. Notice quit will be sent twice.
void test_hello2()
{
   net::io_context ioc;
   auto db = std::make_shared<client_type>(ioc.get_executor());

   auto on_write = [db](std::size_t)
   {
      // Notice this causes a loop, but since quit stops the
      // connection it is not a problem.
      db->send(command::quit);
   };

   db->set_write_handler(on_write);

   net::co_spawn(ioc.get_executor(), run1(db), net::detached);
   net::co_spawn(ioc.get_executor(), reader2(db), net::detached);
   ioc.run();
}

//----------------------------------------------------------------

net::awaitable<void> reader3(std::shared_ptr<client_type> db)
{
   {
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_error(ec, error_code{});
      expect_eq(cmd, command::hello);
   }

   {
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_error(ec, error_code{});
      expect_eq(cmd, command::invalid);
      db->send(command::quit);
   }

   {
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_error(ec, error_code{});
      expect_eq(cmd, command::quit);
   }
}

void test_push()
{
   net::io_context ioc;
   auto db = std::make_shared<client_type>(ioc.get_executor());

   auto on_write = [b = true, db](std::size_t) mutable
   {
      if (std::exchange(b, false))
         db->send(command::subscribe, "channel");
   };

   db->set_write_handler(on_write);

   net::co_spawn(ioc.get_executor(), run1(db), net::detached);
   net::co_spawn(ioc.get_executor(), reader3(db), net::detached);
   ioc.run();
}

//----------------------------------------------------------------

struct receiver4 {
public:
   receiver4(client_type& db) : db_{&db} {}

   void on_read(command cmd)
   {
      if (cmd == command::invalid) {
         db_->send(command::quit);
      } else {
         // Notice this causes a loop.
         db_->send(command::subscribe, "channel");
      }
   }

private:
   client_type* db_;
};

net::awaitable<void> reader4(std::shared_ptr<client_type> db)
{
   {
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_error(ec, error_code{});
      expect_eq(cmd, command::hello);
      db->send(command::subscribe, "channel");
   }

   {
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_error(ec, error_code{});
      expect_eq(cmd, command::invalid);
      db->send(command::quit);
   }

   {
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_error(ec, error_code{});
      expect_eq(cmd, command::quit);
   }
}

void test_push2()
{
   net::io_context ioc;
   auto db = std::make_shared<client_type>(ioc.get_executor());
   net::co_spawn(ioc.get_executor(), run1(db), net::detached);
   net::co_spawn(ioc.get_executor(), reader4(db), net::detached);
   ioc.run();
}

//----------------------------------------------------------------

net::awaitable<void> reader5(std::shared_ptr<client_type> db)
{
   {
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_error(ec, error_code{});
      expect_eq(cmd, command::hello);
   }

   {
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_error(ec, error_code{});
      expect_eq(cmd, command::quit);
   }

   {
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_error(ec, boost::asio::experimental::channel_errc::channel_cancelled);
   }

   {
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_error(ec, error_code{});
      expect_eq(cmd, command::hello);
   }

   {
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_error(ec, error_code{});
      expect_eq(cmd, command::quit);
   }

   {
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_error(ec, boost::asio::experimental::channel_errc::channel_cancelled);
   }
}

net::awaitable<void> run5(std::shared_ptr<client_type> db)
{
   {
      auto [ec] = co_await db->async_run(as_tuple(net::use_awaitable));
      expect_error(ec, net::error::misc_errors::eof);
   }

   {
      auto [ec] = co_await db->async_run(as_tuple(net::use_awaitable));
      expect_error(ec, net::error::misc_errors::eof);
   }
}

// Test whether the client works after a reconnect.
void test_reconnect()
{
   net::io_context ioc;
   auto db = std::make_shared<client_type>(ioc.get_executor());

   auto on_write = [i = 0, db](std::size_t) mutable
   {
      if (i++ < 3)
         db->send(command::quit);
   };

   db->set_write_handler(on_write);

   net::co_spawn(ioc.get_executor(), run5(db), net::detached);
   net::co_spawn(ioc.get_executor(), reader5(db), net::detached);
   ioc.run();
}

net::awaitable<void>
reader6(std::shared_ptr<client_type> db)
{
   {
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_error(ec, error_code{});
      expect_eq(cmd, command::hello);
      db->send(command::quit);
   }

   {
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_error(ec, error_code{});
      expect_eq(cmd, command::quit);
   }

   {
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_error(ec, boost::asio::experimental::channel_errc::channel_cancelled);
   }

   {
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_error(ec, error_code{});
      expect_eq(cmd, command::hello);
      db->send(command::quit);
   }

   {
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_error(ec, error_code{});
      expect_eq(cmd, command::quit);
   }

   {
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_error(ec, boost::asio::experimental::channel_errc::channel_cancelled);
   }
}

void test_reconnect2()
{
   net::io_context ioc;
   auto db = std::make_shared<client_type>(ioc.get_executor());
   net::co_spawn(ioc.get_executor(), run5(db), net::detached);
   net::co_spawn(ioc.get_executor(), reader6(db), net::detached);
   ioc.run();
}

net::awaitable<void> reader7(std::shared_ptr<client_type> db)
{
   int resp = 0;

   auto f = [adapter = adapt(resp)]
            (command cmd, node_type const& nd, error_code& ec) mutable
   {
      if (cmd == command::incr && nd.data_type == aedis::resp3::type::number)
         adapter(nd, ec);
   };

   db->set_adapter(f);

   {
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_error(ec, error_code{});
      expect_eq(cmd, command::hello);
      db->send(command::del, "key");
      db->send(command::multi);
      db->send(command::ping, "aaa");
      db->send(command::incr, "key");
      db->send(command::ping, "bbb");
      db->send(command::discard);
      db->send(command::ping, "ccc");
      db->send(command::incr, "key");
      db->send(command::quit);
   }

   { // del
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_error(ec, error_code{});
      expect_eq(cmd, command::del);
   }

   { // multi
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_error(ec, error_code{});
      expect_eq(cmd, command::multi);
   }

   { // ping
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_error(ec, error_code{});
      expect_eq(cmd, command::ping);
   }

   { // incr
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_error(ec, error_code{});
      expect_eq(cmd, command::incr);
   }

   { // ping
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_error(ec, error_code{});
      expect_eq(cmd, command::ping);
   }

   { // discard
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_error(ec, error_code{});
      expect_eq(cmd, command::discard);
   }

   { // ping
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_error(ec, error_code{});
      expect_eq(cmd, command::ping);
   }

   { // incr
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_error(ec, error_code{});
      expect_eq(cmd, command::incr);
   }

   { // quit
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_error(ec, error_code{});
      expect_eq(cmd, command::quit);
   }

   expect_eq(resp, 1);
}

void test_discard()
{
   net::io_context ioc;
   auto db = std::make_shared<client_type>(ioc.get_executor());
   net::co_spawn(ioc.get_executor(), run1(db), net::detached);
   net::co_spawn(ioc.get_executor(), reader7(db), net::detached);
   ioc.run();
}

struct receiver8 {
public:
   receiver8(client_type& db) : db_{&db} {}

   void on_write(std::size_t)
   {
      if (!std::exchange(sent_, true)) {
         db_->send(command::del, "key");
         db_->send(command::client, "PAUSE", 5000);
      }
   }

private:
   bool sent_ = false;
   client_type* db_;
};

net::awaitable<void> reader8(std::shared_ptr<client_type> db)
{
   {
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_error(ec, error_code{});
      expect_eq(cmd, command::hello);
      db->send(command::client, "PAUSE", 5000);
   }

   {
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_error(ec, error_code{});
      expect_eq(cmd, command::client);
   }
}

net::awaitable<void> run8(std::shared_ptr<client_type> db)
{
   auto [ec] = co_await db->async_run(as_tuple(net::use_awaitable));
   expect_error(ec, aedis::generic::error::idle_timeout);
}

void test_idle()
{
   client_type::config cfg;
   cfg.resolve_timeout = std::chrono::seconds{1};
   cfg.connect_timeout = std::chrono::seconds{1};
   cfg.read_timeout = std::chrono::seconds{1};
   cfg.write_timeout = std::chrono::seconds{1};
   cfg.idle_timeout = std::chrono::seconds{2};

   net::io_context ioc;
   auto db = std::make_shared<client_type>(ioc.get_executor(), cfg);
   net::co_spawn(ioc.get_executor(), run8(db), net::detached);
   net::co_spawn(ioc.get_executor(), reader8(db), net::detached);
   ioc.run();
}

net::awaitable<void> reader9(std::shared_ptr<client_type> db)
{
   {
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_error(ec, error_code{});
      expect_eq(cmd, command::hello);
      db->send(command::del, "key");
   }

   {
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_error(ec, error_code{});
      expect_eq(cmd, command::del);
   }

   for (int i = 0; i < 10000; ++i) {
      db->send(command::incr, "key");
      db->send(command::subscribe, "channel");
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_neq(cmd, command::ping);
   }

   db->send(command::quit);

   for (;;) {
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      expect_neq(cmd, command::ping);
      if (ec)
         co_return;
   }
}

void test_no_ping()
{
   client_type::config cfg;
   cfg.idle_timeout = std::chrono::seconds{2};

   net::io_context ioc;
   auto db = std::make_shared<client_type>(ioc.get_executor(), cfg);
   net::co_spawn(ioc.get_executor(), run1(db), net::detached);
   net::co_spawn(ioc.get_executor(), reader9(db), net::detached);
   ioc.run();
}

int main()
{
   test_resolve_error();
   test_connect_error();
   test_hello();
   test_hello2();
   test_push();
   test_push2();
   test_reconnect();
   test_reconnect2();
   test_discard();
   test_no_ping();

   // Must come last as it send a client pause.
   test_idle();
}

