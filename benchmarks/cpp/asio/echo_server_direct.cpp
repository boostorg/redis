/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/write.hpp>
#include <cstdio>

namespace net = boost::asio;
namespace this_coro = net::this_coro;
using net::ip::tcp;
using net::awaitable;
using net::co_spawn;
using net::detached;
using net::use_awaitable;

awaitable<void> echo(tcp::socket socket)
{
  try {
     char data[1024];
     for (;;) {
        std::size_t n = co_await socket.async_read_some(net::buffer(data), use_awaitable);
        co_await async_write(socket, net::buffer(data, n), use_awaitable);
     }
  } catch (std::exception const& e) {
     //std::printf("echo Exception: %s\n", e.what());
  }
}

awaitable<void> listener()
{
  auto executor = co_await this_coro::executor;
  tcp::acceptor acceptor(executor, {tcp::v4(), 55555});
  for (;;) {
     tcp::socket socket = co_await acceptor.async_accept(use_awaitable);
     co_spawn(executor, echo(std::move(socket)), detached);
  }
}

int main()
{
  try {
     net::io_context io_context(1);
     co_spawn(io_context, listener(), detached);
     io_context.run();
  } catch (std::exception const& e) {
     std::printf("Exception: %s\n", e.what());
  }
}
