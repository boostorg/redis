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
#include "print.hpp"

// Include this in no more than one .cpp file.
#include <aedis/src.hpp>

#if defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)

namespace net = boost::asio;
using aedis::resp3::request;
using aedis::adapt;
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

net::awaitable<void> push_receiver(std::shared_ptr<connection> db)
{
   for (response_type resp;;) {
      co_await db->async_receive_push(adapt(resp));
      print_push(resp);
      resp.clear();
   }
}

net::awaitable<void> event_receiver(std::shared_ptr<connection> db)
{
   request req;
   req.push("SUBSCRIBE", "chat-channel");

   for (;;) {
      auto const ev = co_await db->async_receive_event();
      switch (ev) {
         case connection::event::hello:
         // Subscribes to the channels when a new connection is
         // stablished.
         co_await db->async_exec(req);
         break;

         default:;
      }
   }
}

net::awaitable<void>
publisher(net::posix::stream_descriptor& in, std::shared_ptr<connection> db)
{
   for (std::string msg;;) {
      std::size_t n = co_await net::async_read_until(in, net::dynamic_buffer(msg, 1024), "\n", net::use_awaitable);
      request req;
      req.push("PUBLISH", "chat-channel", msg);
      co_await db->async_exec(req);
      msg.erase(0, n);
   }
}

int main()
{
   try {
      net::io_context ioc{1};
      net::posix::stream_descriptor in{ioc, ::dup(STDIN_FILENO)};

      auto db = std::make_shared<connection>(ioc);
      db->get_config().enable_events = true;

      co_spawn(ioc, publisher(in, db), net::detached);
      co_spawn(ioc, push_receiver(db), net::detached);
      co_spawn(ioc, event_receiver(db), net::detached);
      db->async_run([](auto ec) {
         std::cout << ec.message() << std::endl;
      });

      net::signal_set signals(ioc, SIGINT, SIGTERM);
      signals.async_wait([&](auto, auto){ ioc.stop(); });

      ioc.run();
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}

#else // defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
int main() {}
#endif // defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
