/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <string>
#include <vector>
#include <iostream>
#include <tuple>

#include <boost/asio.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT)
#include <aedis.hpp>
#include "print.hpp"

// Include this in no more than one .cpp file.
#include <aedis/src.hpp>

namespace net = boost::asio;
using aedis::adapt;
using aedis::resp3::request;
using aedis::resp3::node;
using aedis::endpoint;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::socket>;
using stimer = net::use_awaitable_t<>::as_default_on_t<net::steady_timer>;
using connection = aedis::connection<tcp_socket>;

/* This example will subscribe and read pushes indefinitely.
 *
 * To test send messages with redis-cli
 *
 *    $ redis-cli -3
 *    127.0.0.1:6379> PUBLISH channel some-message
 *    (integer) 3
 *    127.0.0.1:6379>
 *
 * To test reconnection try, for example, to close all clients currently
 * connected to the Redis instance
 *
 * $ redis-cli
 * > CLIENT kill TYPE pubsub
 */

// Receives pushes.
net::awaitable<void> push_receiver(std::shared_ptr<connection> db)
{
   for (std::vector<node<std::string>> resp;;) {
      co_await db->async_receive_push(adapt(resp));
      print_push(resp);
      resp.clear();
   }
}

// See
// - https://redis.io/docs/manual/sentinel.
// - https://redis.io/docs/reference/sentinel-clients.
net::awaitable<void> reconnect(std::shared_ptr<connection> db)
{
   request req;
   req.push("SUBSCRIBE", "channel");

   stimer timer{co_await net::this_coro::executor};
   endpoint ep{"127.0.0.1", "6379"};
   for (;;) {
      boost::system::error_code ec;
      co_await db->async_run(ep, req, adapt(), {}, net::redirect_error(net::use_awaitable, ec));
      db->reset_stream();
      std::cout << ec.message() << std::endl;
      timer.expires_after(std::chrono::seconds{1});
      co_await timer.async_wait();
   }
}

int main()
{
   try {
      net::io_context ioc;
      auto db = std::make_shared<connection>(ioc);

      net::co_spawn(ioc, push_receiver(db), net::detached);
      net::co_spawn(ioc, reconnect(db), net::detached);

      net::signal_set signals(ioc, SIGINT, SIGTERM);
      signals.async_wait([&](auto, auto){ ioc.stop(); });

      ioc.run();
   } catch (std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
   }
}

#else // defined(BOOST_ASIO_HAS_CO_AWAIT)
int main() {std::cout << "Requires coroutine support." << std::endl; return 1;}
#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
