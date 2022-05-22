/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <iostream>
#include <boost/asio.hpp>

namespace net = boost::asio;

using net::ip::tcp;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::socket>;
using timer_type = net::use_awaitable_t<>::as_default_on_t<net::steady_timer>;

net::awaitable<void>
example(boost::asio::ip::tcp::endpoint ep, std::string msg, int n)
{
   try {
      auto ex = co_await net::this_coro::executor;

      tcp_socket socket{ex};
      co_await socket.async_connect(ep);

      std::string buffer;
      auto dbuffer = net::dynamic_buffer(buffer);
      for (int i = 0; i < n; ++i) {
         co_await net::async_write(socket, net::buffer(msg));
         auto n = co_await net::async_read_until(socket, dbuffer, "\n");
         //std::printf("> %s", buffer.data());
         dbuffer.consume(n);
      }

      std::printf("Ok: %s", msg.data());
   } catch (std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
   }
}

int main()
{
   try {
      net::io_context ioc;

      tcp::resolver resv{ioc};
      auto const res = resv.resolve("127.0.0.1", "55555");
      auto ep = *std::begin(res);

      int n = 100;
      for (int i = 0; i < 2; ++i)
         net::co_spawn(ioc, example(ep, "Some message\n", n), net::detached);

      ioc.run();
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
