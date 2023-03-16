/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/run.hpp>
#include <boost/redis/check_health.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <unistd.h>
#include <iostream>

#if defined(BOOST_ASIO_HAS_CO_AWAIT)
#include <boost/asio/experimental/awaitable_operators.hpp>
#if defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)

namespace net = boost::asio;
using namespace net::experimental::awaitable_operators;
using stream_descriptor = net::use_awaitable_t<>::as_default_on_t<net::posix::stream_descriptor>;
using signal_set = net::use_awaitable_t<>::as_default_on_t<net::signal_set>;
using boost::redis::request;
using boost::redis::generic_response;
using boost::redis::async_check_health;
using boost::redis::async_run;
using boost::redis::address;
using connection = net::use_awaitable_t<>::as_default_on_t<boost::redis::connection>;

// Chat over Redis pubsub. To test, run this program from multiple
// terminals and type messages to stdin.

// Receives Redis pushes.
auto receiver(std::shared_ptr<connection> conn) -> net::awaitable<void>
{
   for (generic_response resp;;) {
      co_await conn->async_receive(resp);
      std::cout << resp.value().at(1).value << " " << resp.value().at(2).value << " " << resp.value().at(3).value << std::endl;
      resp.value().clear();
   }
}

// Publishes stdin messages to a Redis channel.
auto publisher(std::shared_ptr<stream_descriptor> in, std::shared_ptr<connection> conn) -> net::awaitable<void>
{
   for (std::string msg;;) {
      auto n = co_await net::async_read_until(*in, net::dynamic_buffer(msg, 1024), "\n");
      request req;
      req.push("PUBLISH", "chat-channel", msg);
      co_await conn->async_exec(req);
      msg.erase(0, n);
   }
}

// Called from the main function (see main.cpp)
auto co_main(address const& addr) -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;
   auto conn = std::make_shared<connection>(ex);
   auto stream = std::make_shared<stream_descriptor>(ex, ::dup(STDIN_FILENO));
   signal_set sig{ex, SIGINT, SIGTERM};

   request req;
   req.push("HELLO", 3);
   req.push("SUBSCRIBE", "chat-channel");

   co_await ((async_run(*conn, addr) || publisher(stream, conn) || receiver(conn) ||
         async_check_health(*conn) || sig.async_wait()) &&
         conn->async_exec(req));
}

#else // defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
auto co_main(address const&) -> net::awaitable<void>
{
   std::cout << "Requires support for posix streams." << std::endl;
   co_return;
}
#endif // defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
