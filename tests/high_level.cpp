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

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

#include "check.hpp"

//std::cout << "aaaa " << ec.message() << " " << cmd << " " << n << std::endl;

namespace net = boost::asio;
namespace resp3 = aedis::resp3;

using aedis::adapter::adapt;
using aedis::adapter::adapter_t;
using aedis::redis::command;
using aedis::resp3::node;
using aedis::generic::request;
using connection = aedis::generic::connection<command>;
using error_code = boost::system::error_code;
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
   connection::config cfg;
   cfg.host = "Atibaia";
   connection db(ioc.get_executor(), adapt(), cfg);
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
   connection::config cfg;
   cfg.port = "1";
   connection db(ioc.get_executor(), adapt(), cfg);
   db.async_run(f);
   ioc.run();
}

//----------------------------------------------------------------

// Test if quit make async_run exit.
void test_quit()
{
   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc.get_executor());

   request<command> req;
   req.push(command::quit);
   db->async_exec(req, [](auto ec, auto r){
      expect_no_error(ec);
      //expect_eq(w, 36UL);
      //expect_eq(r, 152UL);
   });

   db->async_run([](auto ec){
      expect_error(ec, net::error::misc_errors::eof);
   });

   ioc.run();
}

//----------------------------------------------------------------

net::awaitable<void>
push_consumer3(std::shared_ptr<connection> db)
{
   {
      auto [ec, n] = co_await db->async_read_push(as_tuple(net::use_awaitable));
      expect_no_error(ec);
   }

   {
      auto [ec, n] = co_await db->async_read_push(as_tuple(net::use_awaitable));
      expect_error(ec, net::experimental::channel_errc::channel_cancelled);
   }
}

void test_push()
{
   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc.get_executor());

   request<command> req;
   req.push(command::subscribe, "channel");
   req.push(command::quit);

   db->async_exec(req, [](auto ec, auto r){
      expect_no_error(ec);
      //expect_eq(w, 68UL);
      //expect_eq(r, 151UL);
   });

   net::co_spawn(ioc.get_executor(), push_consumer3(db), net::detached);

   db->async_run([](auto ec){
      expect_error(ec, net::error::misc_errors::eof);
   });

   ioc.run();
}

////----------------------------------------------------------------

net::awaitable<void> run5(std::shared_ptr<connection> db)
{
   {
      request<command> req;
      req.push(command::quit);
      db->async_exec(req, [](auto ec, auto){
         expect_no_error(ec);
      });

      auto [ec] = co_await db->async_run(as_tuple(net::use_awaitable));
      expect_error(ec, net::error::misc_errors::eof);
   }

   {
      request<command> req;
      req.push(command::quit);
      db->async_exec(req, [](auto ec, auto){
         expect_no_error(ec);
      });

      auto [ec] = co_await db->async_run(as_tuple(net::use_awaitable));
      expect_error(ec, net::error::misc_errors::eof);
   }
}

// Test whether the client works after a reconnect.
void test_reconnect()
{
   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc.get_executor());

   net::co_spawn(ioc.get_executor(), run5(db), net::detached);
   ioc.run();
}

void test_idle()
{
   connection::config cfg;
   cfg.resolve_timeout = std::chrono::seconds{1};
   cfg.connect_timeout = std::chrono::seconds{1};
   cfg.read_timeout = std::chrono::seconds{1};
   cfg.write_timeout = std::chrono::seconds{1};
   cfg.ping_delay_timeout = std::chrono::seconds{1};

   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc.get_executor(), adapt(), cfg);

   request<command> req;
   req.push(command::client, "PAUSE", 5000);

   db->async_exec(req, [](auto ec, auto r){
      expect_no_error(ec);
   });

   db->async_run([](auto ec){
      expect_error(ec, aedis::generic::error::idle_timeout);
   });

   ioc.run();
}

int main()
{
   test_resolve_error();
   test_connect_error();
   test_quit();
   test_push();
   //test_reconnect();

   // Must come last as it send a client pause.
   test_idle();
}

