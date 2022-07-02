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

using aedis::resp3::request;
using connection = aedis::connection<>;
using error_code = boost::system::error_code;
using net::experimental::as_tuple;
using tcp = net::ip::tcp;
using boost::system::error_code;

auto print_read = [](auto cmd, auto n)
{
   std::cout << cmd << ": " << n << std::endl;
};

//----------------------------------------------------------------

void test_resolve_error()
{
   net::io_context ioc;
   connection db(ioc);
   db.async_run("Atibaia", "6379", [](auto ec) {
      expect_error(ec, net::error::netdb_errors::host_not_found, "test_resolve_error");
   });
   ioc.run();
}

//----------------------------------------------------------------

void test_connect_error()
{
   net::io_context ioc;
   connection db(ioc);
   db.async_run("127.0.0.1", "1", [](auto ec) {
      expect_error(ec, net::error::basic_errors::connection_refused, "test_connect_error");
   });
   ioc.run();
}

//----------------------------------------------------------------

// Test if quit causes async_run to exit.
void test_quit()
{
   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc);

   request req;
   req.push("QUIT");
   db->async_exec(req, aedis::adapt(), [](auto ec, auto r){
      expect_no_error(ec, "test_quit");
   });

   db->async_run("127.0.0.1", "6379", [](auto ec){
      expect_error(ec, net::error::misc_errors::eof, "test_quit");
   });

   ioc.run();
}

void test_quit2()
{
   request req;
   req.push("QUIT");

   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc);
   db->async_exec("127.0.0.1", "6379", req, aedis::adapt(), [](auto ec, auto n){
      expect_error(ec, net::error::misc_errors::eof, "test_quit2");
   });

   ioc.run();
}

//----------------------------------------------------------------

net::awaitable<void>
push_consumer1(std::shared_ptr<connection> db)
{
   {
      auto [ec, n] = co_await db->async_read_push(aedis::adapt(), as_tuple(net::use_awaitable));
      expect_no_error(ec, "push_consumer");
   }

   {
      auto [ec, n] = co_await db->async_read_push(aedis::adapt(), as_tuple(net::use_awaitable));
      expect_error(ec, boost::asio::experimental::channel_errc::channel_cancelled, "push_consumer1");
   }
}

// Tests whether a push is indeed delivered.
void test_push1()
{
   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc);

   request req;
   req.push("SUBSCRIBE", "channel");
   req.push("QUIT");

   db->async_exec("127.0.0.1", "6379", req, aedis::adapt(), [](auto ec, auto r){
      expect_error(ec, net::error::misc_errors::eof, "test_push1");
   });

   net::co_spawn(ioc.get_executor(), push_consumer1(db), net::detached);
   ioc.run();
}

////----------------------------------------------------------------

net::awaitable<void> run5(std::shared_ptr<connection> db)
{
   {
      request req;
      req.push("QUIT");
      db->async_exec("127.0.0.1", "6379", req, aedis::adapt(), [](auto ec, auto){
         // TODO: This should be eof.
         expect_error(ec, net::error::basic_errors::operation_aborted, "run5a");
      });
   }

   {
      request req;
      req.push("QUIT");
      db->async_exec("127.0.0.1", "6379", req, aedis::adapt(), [](auto ec, auto){
         expect_error(ec, net::error::misc_errors::eof, "run5b");
      });
   }

   co_return;
}

// Test whether the client works after a reconnect.
void test_reconnect()
{
   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc.get_executor());

   net::co_spawn(ioc, run5(db), net::detached);
   ioc.run();
}

// Checks whether we get idle timeout when no push reader is set.
void test_no_push_reader1()
{
   connection::config cfg;

   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc, cfg);

   request req;
   req.push("SUBSCRIBE", "channel");

   db->async_exec("127.0.0.1", "6379", req, aedis::adapt(), [](auto ec, auto r){
      expect_error(ec, aedis::error::idle_timeout, "test_no_push_reader1");
   });

   ioc.run();
}

void test_no_push_reader2()
{
   connection::config cfg;

   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc, cfg);

   request req;
   // Wrong command.
   req.push("SUBSCRIBE");

   db->async_exec("127.0.0.1", "6379", req, aedis::adapt(), [](auto ec, auto r){
      expect_error(ec, aedis::error::idle_timeout, "test_no_push_reader2");
   });

   ioc.run();
}

void test_no_push_reader3()
{
   connection::config cfg;

   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc, cfg);

   request req;
   // Wrong command.
   req.push("PING", "Message");
   req.push("SUBSCRIBE");

   db->async_exec("127.0.0.1", "6379", req, aedis::adapt(), [](auto ec, auto r){
      expect_error(ec, aedis::error::idle_timeout, "test_no_push_reader3");
   });

   ioc.run();
}

void test_idle()
{
   connection::config cfg;
   cfg.resolve_timeout = std::chrono::seconds{1};
   cfg.connect_timeout = std::chrono::seconds{1};
   cfg.ping_interval = std::chrono::seconds{1};

   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc, cfg);

   request req;
   req.push("CLIENT", "PAUSE", 5000);

   db->async_exec(req, aedis::adapt(), [](auto ec, auto r){
      expect_no_error(ec, "test_idle");
   });

   db->async_run("127.0.0.1", "6379", [](auto ec){
      expect_error(ec, aedis::error::idle_timeout, "test_idle");
   });

   ioc.run();
}

auto handler =[](auto ec, auto...)
   { std::cout << ec.message() << std::endl; };

void test_push2()
{
   request req1;
   req1.push("PING", "Message1");

   request req2;
   req2.push("SUBSCRIBE", "channel");

   request req3;
   req3.push("PING", "Message2");
   req3.push("QUIT");

   std::tuple<std::string, std::string> resp;

   net::io_context ioc;
   connection db{ioc};
   db.async_exec(req1, aedis::adapt(resp), handler);
   db.async_exec(req2, aedis::adapt(resp), handler);
   db.async_exec(req3, aedis::adapt(resp), handler);
   db.async_run("127.0.0.1", "6379", handler);
   ioc.run();
}

net::awaitable<void>
push_consumer3(std::shared_ptr<connection> db)
{
   for (;;)
      co_await db->async_read_push(aedis::adapt(), net::use_awaitable);
}

void test_push3()
{
   request req1;
   req1.push("PING", "Message1");

   request req2;
   req2.push("SUBSCRIBE", "channel");

   request req3;
   req3.push("QUIT");

   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc);
   db->async_exec(req1, aedis::adapt(), handler);
   db->async_exec(req2, aedis::adapt(), handler);
   db->async_exec(req2, aedis::adapt(), handler);
   db->async_exec(req1, aedis::adapt(), handler);
   db->async_exec(req2, aedis::adapt(), handler);
   db->async_exec(req1, aedis::adapt(), handler);
   db->async_exec(req2, aedis::adapt(), handler);
   db->async_exec(req2, aedis::adapt(), handler);
   db->async_exec(req1, aedis::adapt(), handler);
   db->async_exec(req2, aedis::adapt(), handler);
   db->async_exec(req3, aedis::adapt(), handler);
   db->async_run("127.0.0.1", "6379", handler);
   net::co_spawn(ioc.get_executor(), push_consumer3(db), net::detached);
   ioc.run();
}

void test_exec_while_processing()
{
   request req1;
   req1.push("PING", "Message1");

   request req2;
   req2.push("SUBSCRIBE", "channel");

   request req3;
   req3.push("QUIT");

   net::io_context ioc;
   auto db = std::make_shared<connection>(ioc);

   db->async_exec(req1, aedis::adapt(), [db, &req1](auto ec, auto) {
      db->async_exec(req1, aedis::adapt(), handler);
   });

   db->async_exec(req1, aedis::adapt(), [db, &req2](auto ec, auto) {
      db->async_exec(req2, aedis::adapt(), handler);
   });

   db->async_exec(req2, aedis::adapt(), [db, &req2](auto ec, auto) {
      db->async_exec(req2, aedis::adapt(), handler);
   });

   db->async_exec(req1, aedis::adapt(), [db, &req1](auto ec, auto) {
      db->async_exec(req1, aedis::adapt(), handler);
   });

   db->async_exec(req2, aedis::adapt(), [db, &req3](auto ec, auto) {
      db->async_exec(req3, aedis::adapt(), handler);
   });

   db->async_run("127.0.0.1", "6379", handler);
   net::co_spawn(ioc.get_executor(), push_consumer3(db), net::detached);
   ioc.run();
}

int main()
{
   test_resolve_error();
   test_connect_error();
   test_quit();
   test_quit2();
   test_push1();
   test_push2();
   test_push3();
   test_no_push_reader1();
   test_no_push_reader2();
   test_no_push_reader3();
   //test_reconnect();
   test_exec_while_processing();

   // Must come last as it send a client pause.
   test_idle();
}

