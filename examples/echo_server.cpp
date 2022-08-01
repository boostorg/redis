/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <string>
#include <iostream>
#include <boost/asio.hpp>
#include <aedis.hpp>

// Include this in no more than one .cpp file.
#include <aedis/src.hpp>

namespace net = boost::asio;
using aedis::adapt;
using aedis::resp3::request;
using executor_type = net::io_context::executor_type;
using socket_type = net::basic_stream_socket<net::ip::tcp, executor_type>;
using tcp_socket = net::use_awaitable_t<executor_type>::as_default_on_t<socket_type>;
using acceptor_type = net::basic_socket_acceptor<net::ip::tcp, executor_type>;
using tcp_acceptor = net::use_awaitable_t<executor_type>::as_default_on_t<acceptor_type>;
using awaitable_type = net::awaitable<void, executor_type>;
using connection = aedis::connection<tcp_socket>;

awaitable_type echo_loop(tcp_socket socket, std::shared_ptr<connection> db)
{
   try {
      request req;
      std::tuple<std::string> resp;
      std::string buffer;

      for (;;) {
         auto n = co_await net::async_read_until(socket, net::dynamic_buffer(buffer, 1024), "\n");
         req.push("PING", buffer);
         co_await db->async_exec(req, adapt(resp));
         co_await net::async_write(socket, net::buffer(std::get<0>(resp)));
         std::get<0>(resp).clear();
         req.clear();
         buffer.erase(0, n);
      }
   } catch (std::exception const& e) {
      std::cout << e.what() << std::endl;
   }
}

awaitable_type listener()
{
   auto ex = co_await net::this_coro::executor;
   auto db = std::make_shared<connection>(ex);
   db->async_run(net::detached);

   request req;
   req.push("HELLO", 3);
   co_await db->async_exec(req);

   tcp_acceptor acc(ex, {net::ip::tcp::v4(), 55555});
   for (;;)
      net::co_spawn(ex, echo_loop(co_await acc.async_accept(), db), net::detached);
}

int main()
{
   try {
      net::io_context ioc{BOOST_ASIO_CONCURRENCY_HINT_UNSAFE_IO};
      co_spawn(ioc, listener(), net::detached);
      ioc.run();
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
