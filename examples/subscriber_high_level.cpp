/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <iostream>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/experimental/as_tuple.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

namespace net = boost::asio;
using aedis::adapter::adapt;
using aedis::sentinel::command;
using aedis::generic::client;
using net::experimental::as_tuple;
using client_type = client<net::ip::tcp::socket, command>;
using node_type = aedis::resp3::node<boost::string_view>;
using response_type = std::vector<aedis::resp3::node<std::string>>;
using namespace net::experimental::awaitable_operators;

/* In this example we send a subscription to a channel and start
 * reading server side messages indefinitely.
 *
 * After starting the example you can test it by sending messages with
 * redis-cli like this
 *
 *    $ redis-cli -3
 *    127.0.0.1:6379> PUBLISH channel1 some-message
 *    (integer) 3
 *    127.0.0.1:6379>
 *
 * The messages will then appear on the terminal you are running the
 * example.
 */

net::awaitable<void>
push_reader(std::shared_ptr<client_type> db, response_type& resp)
{
   for (;;) {
      auto [ec, cmd] = co_await db->async_read_push(as_tuple(net::use_awaitable));

      std::cout
         << "push_reader: " << ec.message() << ", "
         << cmd << std::endl;

      if (ec)
         co_return;

      std::cout
         << "Event: "   << resp.at(1).value << "\n"
         << "Channel: " << resp.at(2).value << "\n"
         << "Message: " << resp.at(3).value << "\n"
         << std::endl;

      resp.clear();
   }
}

net::awaitable<void>
command_reader(std::shared_ptr<client_type> db, response_type& resp)
{
   for (;;) {
      auto [ec, cmd, n] = co_await db->async_read_one(as_tuple(net::use_awaitable));

      std::cout
         << "command_reader: " << ec.message() << ", "
         << cmd << ", " << n << std::endl;

      if (ec)
         co_return;

      if (cmd == command::hello)
         db->send(command::subscribe, "channel1", "channel2");

      resp.clear();
   }
}

net::awaitable<void> run()
{
   auto ex = co_await net::this_coro::executor;
   auto db = std::make_shared<client_type>(ex);

   response_type resp;
   db->set_adapter(adapt(resp));

   co_await (
      db->async_run(net::use_awaitable) &&
      command_reader(db, resp) &&
      push_reader(db, resp)
   );
}

int main()
{
   net::io_context ioc;
   net::co_spawn(ioc.get_executor(), run(), net::detached);
   ioc.run();
}
