/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <string>
#include <iostream>
#include <memory>

#include <boost/asio.hpp>
#include <boost/asio/experimental/as_tuple.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

namespace net = boost::asio;
using aedis::resp3::node;
using aedis::adapter::adapt;
using aedis::redis::command;
using aedis::generic::make_client_adapter;
using net::experimental::as_tuple;
using client_type = aedis::generic::client<net::ip::tcp::socket, command>;
using namespace net::experimental::awaitable_operators;

net::awaitable<void>
reader(std::shared_ptr<client_type> db)
{
   node<std::string> resp;
   db->set_adapter(make_client_adapter<command>(adapt(resp)));

   for (;;) {
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      if (ec)
         co_return;

      switch (cmd) {
         case command::hello:
         db->send(command::ping, "O rato roeu a roupa do rei de Roma");
         db->send(command::incr, "intro-counter");
         db->send(command::set, "intro-key", "Três pratos de trigo para três tigres");
         db->send(command::get, "intro-key");
         db->send(command::quit);
         break;

         default:
         std::cout << resp.value << std::endl;
      }
   }
}

net::awaitable<void> run()
{
   auto ex = co_await net::this_coro::executor;
   auto db = std::make_shared<client_type>(ex);
   co_await (db->async_run(net::use_awaitable) && reader(db));
}

int main()
{
   net::io_context ioc;
   net::co_spawn(ioc.get_executor(), run(), net::detached);
   ioc.run();
}

