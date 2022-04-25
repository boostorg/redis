/* Copyright (c) 2018 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
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

   void on_read(command) {}

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

template <class Receiver>
struct reconnect {
   client_type db;
   Receiver recv;
   boost::asio::steady_timer timer;
   net::coroutine coro;

   reconnect(net::any_io_executor ex)
   : db{ex}
   , recv{db}
   , timer{ex}
   {
      db.set_read_handler([this](auto cmd, auto){recv.on_read(cmd);});
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
   reconnect<receiver5> rec{ioc.get_executor()};
   rec.on_event({});
   ioc.run();
}

struct receiver6 {
public:
   int counter = 0;

   receiver6(client_type& db)
   : db_{&db}
   , adapter_{adapt(counter)}
   {}

   void on_write() {}
   void on_read(command cmd)
   {
      if (cmd == command::hello) {
         db_->send(command::get, "receiver6-key");
         if (counter == 0)
            db_->send(command::del, "receiver6-key");
         db_->send(command::incr, "receiver6-key");
         db_->send(command::quit);
         return;
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

void test_reconnect2()
{
   net::io_context ioc;
   reconnect<receiver6> rec{ioc.get_executor()};
   rec.on_event({});
   ioc.run();
}

struct receiver7 {
public:
   int counter = 0;

   receiver7(client_type& db)
   : db_{&db}
   , adapter_{adapt(counter)}
   {}

   void on_resp3(command cmd, node<boost::string_view> const& nd, boost::system::error_code& ec)
   {
      if (cmd == command::incr)
         adapter_(nd, ec);
   }

   void on_write(std::size_t)
   {
      if (!std::exchange(sent_, true)) {
         db_->send(command::del, "key");
         db_->send(command::multi);
         db_->send(command::ping, "aaa");
         db_->send(command::incr, "key");
         db_->send(command::ping, "bbb");
         db_->send(command::discard);
         db_->send(command::ping, "ccc");
         db_->send(command::incr, "key");
         db_->send(command::quit);
      }
   }

   void on_read(command cmd, std::size_t)
   {
   }

private:
   bool sent_ = false;
   client_type* db_;
   adapter_t<int> adapter_;
};

void test_discard()
{
   auto f = [](auto ec)
   {
      expect_error(ec, net::error::misc_errors::eof);
   };

   net::io_context ioc;
   client_type db(ioc.get_executor());

   receiver7 recv{db};
   db.set_read_handler([&recv](auto cmd, std::size_t n){recv.on_read(cmd, n);});
   db.set_write_handler([&recv](std::size_t n){recv.on_write(n);});
   db.set_resp3_handler([&recv](auto a, auto b, auto c){recv.on_resp3(a, b, c);});

   db.async_run("127.0.0.1", "6379", f);
   ioc.run();

   expect_eq(recv.counter, 1, "test_discard.");
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

void test_idle()
{
   auto f = [](auto ec)
   {
      expect_error(ec, aedis::generic::error::idle_timeout);
   };

   net::io_context ioc;
   client_type::config cfg;
   cfg.resolve_timeout = std::chrono::seconds{1};
   cfg.connect_timeout = std::chrono::seconds{1};
   cfg.read_timeout = std::chrono::seconds{1};
   cfg.write_timeout = std::chrono::seconds{1};
   cfg.idle_timeout = std::chrono::seconds{2};
   client_type db(ioc.get_executor(), cfg);

   receiver8 recv{db};
   db.set_write_handler([&recv](std::size_t n){recv.on_write(n);});

   db.async_run("127.0.0.1", "6379", f);
   ioc.run();
}

struct receiver9 {
public:
   bool ping = false;

   receiver9(client_type& db) : db_{&db} , adapter_{adapt(counter_)} {}

   void on_resp3(command cmd, node<boost::string_view> const& nd, boost::system::error_code& ec)
   {
      if (cmd == command::incr)
         adapter_(nd, ec);
   }

   void on_push(std::size_t) {}

   void on_write(std::size_t)
   {
      if (!std::exchange(sent_, true))
         db_->send(command::del, "key");

      db_->send(command::incr, "key");
      db_->send(command::subscribe, "channel");
   }

   void on_read(command cmd, std::size_t)
   {
      db_->send(command::incr, "key");
      db_->send(command::subscribe, "channel");
      if (counter_ == 100000) {
         std::cout << "Success: counter increase." << std::endl;
         db_->send(command::quit);
      }

      if (cmd == command::ping)
         ping = true;
   }

private:
   bool sent_ = false;
   client_type* db_;
   int counter_ = 0;
   adapter_t<int> adapter_;
};

void test_no_ping()
{
   auto f = [](auto ec)
   {
      expect_error(ec, net::error::misc_errors::eof);
   };

   net::io_context ioc;
   client_type::config cfg;
   cfg.idle_timeout = std::chrono::seconds{2};
   client_type db(ioc.get_executor(), cfg);

   auto recv = std::make_shared<receiver9>(db);
   db.set_receiver(recv);
   db.async_run("127.0.0.1", "6379", f);
   ioc.run();

   expect_eq(recv->ping, false, "No ping received.");
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

