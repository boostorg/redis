/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres.gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

namespace resp3 = aedis::resp3;
using aedis::redis::command;
using aedis::redis::make_serializer;
using aedis::adapter::adapt;

namespace net = aedis::net;
using net::ip::tcp;
using net::write;
using net::buffer;
using net::dynamic_buffer;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::socket>;

using response_type = std::tuple<std::string, std::optional<std::string>>;

net::awaitable<response_type> set(net::ip::tcp::endpoint ep)
{
   auto ex = co_await net::this_coro::executor;

   tcp_socket socket{ex};
   co_await socket.async_connect(ep);

   std::string request;
   auto sr = make_serializer(request);
   sr.push(command::hello, 3);
   sr.push(command::multi);
   sr.push(command::ping, "Some message.");
   sr.push(command::set, "low-level-key", "some content", "EX", "2", "get");
   sr.push(command::exec);
   sr.push(command::quit);
   co_await net::async_write(socket, buffer(request));

   std::tuple<std::string, std::optional<std::string>> response;

   std::string buffer;
   co_await resp3::async_read(socket, dynamic_buffer(buffer)); // hello
   co_await resp3::async_read(socket, dynamic_buffer(buffer)); // multi
   co_await resp3::async_read(socket, dynamic_buffer(buffer)); // ping
   co_await resp3::async_read(socket, dynamic_buffer(buffer)); // set
   co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt(response));
   co_await resp3::async_read(socket, dynamic_buffer(buffer));

   co_return response;
}

net::awaitable<std::string> low_level()
{
   auto ex = co_await net::this_coro::executor;
   tcp::resolver resv{ex};
   auto const res = resv.resolve("127.0.0.1", "6379");
   auto const response = co_await net::co_spawn(ex, set(*res.begin()), net::use_awaitable);

   std::cout
      << "Ping: " << std::get<0>(response) << "\n"
      << "Get (has_value): " << std::get<1>(response).has_value()
      << std::endl;

   if (std::get<1>(response).has_value())
      std::cout << "Get (value): " << std::get<1>(response).value() << std::endl;
}

int main()
{
   try {
      net::io_context ioc;
      net::co_spawn(ioc, low_level(), net::detached);
      ioc.run();
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}

