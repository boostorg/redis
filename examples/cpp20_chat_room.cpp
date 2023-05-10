/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <unistd.h>
#include <iostream>

#if defined(BOOST_ASIO_HAS_CO_AWAIT)
#if defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)

namespace net = boost::asio;
using stream_descriptor = net::deferred_t::as_default_on_t<net::posix::stream_descriptor>;
using connection = net::deferred_t::as_default_on_t<boost::redis::connection>;
using signal_set = net::deferred_t::as_default_on_t<net::signal_set>;
using boost::redis::request;
using boost::redis::generic_response;
using boost::redis::config;
using net::redirect_error;
using net::use_awaitable;
using boost::system::error_code;
using namespace std::chrono_literals;

// Chat over Redis pubsub. To test, run this program from multiple
// terminals and type messages to stdin.

auto
receiver(std::shared_ptr<connection> conn) -> net::awaitable<void>
{
   request req;
   req.push("SUBSCRIBE", "channel");

   while (conn->will_reconnect()) {

      // Subscribe to channels.
      co_await conn->async_exec(req);

      // Loop reading Redis push messages.
      for (generic_response resp;;) {
         error_code ec;
         co_await conn->async_receive(resp, redirect_error(use_awaitable, ec));
         if (ec)
            break; // Connection lost, break so we can reconnect to channels.
         std::cout
            << resp.value().at(1).value
            << " " << resp.value().at(2).value
            << " " << resp.value().at(3).value
            << std::endl;
         resp.value().clear();
      }
   }
}

// Publishes stdin messages to a Redis channel.
auto publisher(std::shared_ptr<stream_descriptor> in, std::shared_ptr<connection> conn) -> net::awaitable<void>
{
   for (std::string msg;;) {
      auto n = co_await net::async_read_until(*in, net::dynamic_buffer(msg, 1024), "\n");
      request req;
      req.push("PUBLISH", "channel", msg);
      co_await conn->async_exec(req);
      msg.erase(0, n);
   }
}

// Called from the main function (see main.cpp)
auto co_main(config cfg) -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;
   auto conn = std::make_shared<connection>(ex);
   auto stream = std::make_shared<stream_descriptor>(ex, ::dup(STDIN_FILENO));

   net::co_spawn(ex, receiver(conn), net::detached);
   net::co_spawn(ex, publisher(stream, conn), net::detached);
   conn->async_run(cfg, {}, net::consign(net::detached, conn));

   signal_set sig_set{ex, SIGINT, SIGTERM};
   co_await sig_set.async_wait();
   conn->cancel();
   stream->cancel();
}

#else // defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
auto co_main(config const&) -> net::awaitable<void>
{
   std::cout << "Requires support for posix streams." << std::endl;
   co_return;
}
#endif // defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
