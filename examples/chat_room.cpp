/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <string>
#include <iostream>
#include <boost/asio.hpp>
#include <aedis.hpp>
#include "unistd.h"

// Include this in no more than one .cpp file.
#include <aedis/src.hpp>

#if defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)

namespace net = boost::asio;
using aedis::adapt;
using aedis::resp3::request;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::socket>;
using tcp_acceptor = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::acceptor>;
using connection = aedis::connection<tcp_socket>;
using response_type = std::vector<aedis::resp3::node<std::string>>;

// Chat over redis pubsub. To test, run this program from different
// terminals and type messages to stdin. You may also want to run
//
//    $ redis-cli
//    > monitor
//
// To see the message traffic.

net::awaitable<void> reader(std::shared_ptr<connection> db)
{
   try {
      request req;
      req.push("SUBSCRIBE", "chat-channel");
      co_await db->async_exec(req);

      for (response_type resp;;) {
         co_await db->async_receive(adapt(resp));
         std::cout << "> " << resp.at(3).value;
         resp.clear();
      }
   } catch (std::exception const&) {
   }
}

net::awaitable<void>
run(net::posix::stream_descriptor& in, std::shared_ptr<connection> db)
{
   try {
      for (std::string msg;;) {
         std::size_t n = co_await net::async_read_until(in, net::dynamic_buffer(msg, 1024), "\n", net::use_awaitable);
         request req;
         req.push("PUBLISH", "chat-channel", msg);
         co_await db->async_exec(req);
         msg.erase(0, n);
      }

   } catch (std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
   }
}

int main()
{
   try {
      net::io_context ioc{1};
      net::posix::stream_descriptor in{ioc, ::dup(STDIN_FILENO)};
      auto db = std::make_shared<connection>(ioc);
      co_spawn(ioc, run(in, db), net::detached);
      co_spawn(ioc, reader(db), net::detached);

      db->async_run([](auto ec) {
         std::cout << ec.message() << std::endl;
      });

      ioc.run();
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}

#else // defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
int main() {}
#endif // defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
