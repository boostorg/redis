/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <iostream>
#include <vector>

#include <boost/asio.hpp>
#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

namespace net = boost::asio;
using aedis::adapter::adapt;
using aedis::redis::command;
using aedis::generic::request;
using client_type = aedis::generic::client<net::ip::tcp::socket, command>;
using response_type = std::vector<aedis::resp3::node<std::string>>;

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
      auto n = co_await db->async_read_push(net::use_awaitable);
      std::cout
         << "Size: "   << n << "\n"
         << "Event: "   << resp.at(1).value << "\n"
         << "Channel: " << resp.at(2).value << "\n"
         << "Message: " << resp.at(3).value << "\n"
         << std::endl;

      resp.clear();
   }
}

auto run_handler =[](auto ec)
{
   std::printf("Run: %s\n", ec.message().data());
};

int main()
{
   response_type resp;

   net::io_context ioc;

   auto db = std::make_shared<client_type>(ioc.get_executor());
   db->set_adapter(adapt(resp));
   net::co_spawn(ioc.get_executor(), push_reader(db, resp), net::detached);

   request<command> req;
   req.push(command::hello, 3);
   req.push(command::subscribe, "channel1", "channel2");
   db->async_exec(req, [&](auto, auto){req.clear();});

   db->async_run(run_handler);
   ioc.run();
}
