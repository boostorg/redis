/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <iostream>

#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/co_spawn.hpp>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

namespace net = boost::asio;
namespace resp3 = aedis::resp3;

using aedis::resp3::node;
using aedis::redis::command;
using aedis::adapter::adapt;
using aedis::generic::serializer;
using net::ip::tcp;
using net::write;
using net::buffer;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::socket>;

net::awaitable<void> example()
{
   auto ex = co_await net::this_coro::executor;

   tcp::resolver resv{ex};
   auto const res = resv.resolve("127.0.0.1", "6379");
   tcp_socket socket{ex};
   co_await socket.async_connect(*std::begin(res));

   serializer<command> req;
   req.push(command::hello, 3);
   req.push(command::subscribe, "channel1", "channel2");
   co_await resp3::async_write(socket, req);

   // Ignores the response to hello.
   std::string buffer;
   co_await resp3::async_read(socket, net::dynamic_buffer(buffer));

   for (std::vector<node<std::string>> resp;;) {
      co_await resp3::async_read(socket, net::dynamic_buffer(buffer), adapt(resp));
      for (auto const& e: resp)
         std::cout << e << std::endl;
      resp.clear();
   }
}

int main()
{
   try {
      net::io_context ioc;
      net::co_spawn(ioc, example(), net::detached);
      ioc.run();
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}

