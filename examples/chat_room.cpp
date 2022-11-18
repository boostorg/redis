/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <string>
#include <iostream>
#include "unistd.h"

#include <boost/asio.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT) && defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <aedis.hpp>
#include "print.hpp"
#include "reconnect.hpp"

// Include this in no more than one .cpp file.
#include <aedis/src.hpp>

namespace net = boost::asio;
using namespace net::experimental::awaitable_operators;
using resolver = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::resolver>;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::socket>;
using tcp_acceptor = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::acceptor>;
using stream_descriptor = net::use_awaitable_t<>::as_default_on_t<net::posix::stream_descriptor>;

using aedis::adapt;
using aedis::resp3::request;
using aedis::resp3::node;

// Chat over redis pubsub. To test, run this program from different
// terminals and type messages to stdin. Use
//
//    $ redis-cli monitor
//
// to monitor the message traffic.

// Receives messages from other users.
auto push_receiver(std::shared_ptr<connection> conn) -> net::awaitable<void>
{
   for (std::vector<node<std::string>> resp;;) {
      co_await conn->async_receive(adapt(resp));
      print_push(resp);
      resp.clear();
   }
}

// Publishes messages to other users.
net::awaitable<void> publisher(stream_descriptor& in, std::shared_ptr<connection> conn)
{
   for (std::string msg;;) {
      auto n = co_await net::async_read_until(in, net::dynamic_buffer(msg, 1024), "\n");
      request req;
      req.push("PUBLISH", "chat-channel", msg);
      co_await conn->async_exec(req);
      msg.erase(0, n);
   }
}

auto main() -> int
{
   try {
      net::io_context ioc{1};
      stream_descriptor in{ioc, ::dup(STDIN_FILENO)};
      auto conn = std::make_shared<connection>(ioc);

      request req;
      req.get_config().cancel_on_connection_lost = true;
      req.push("HELLO", 3);
      req.push("SUBSCRIBE", "chat-channel");

      co_spawn(ioc, publisher(in, conn), net::detached);
      co_spawn(ioc, push_receiver(conn), net::detached);
      co_spawn(ioc, reconnect(conn, req), net::detached);

      net::signal_set signals(ioc, SIGINT, SIGTERM);
      signals.async_wait([&](auto, auto){ ioc.stop(); });

      ioc.run();
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}

#else // defined(BOOST_ASIO_HAS_CO_AWAIT) && defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
auto main() -> int {std::cout << "Requires coroutine support." << std::endl; return 0;}
#endif // defined(BOOST_ASIO_HAS_CO_AWAIT) && defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
