/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include "reconnect.hpp"

#if defined(BOOST_ASIO_HAS_CO_AWAIT)
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <iostream>

namespace net = boost::asio;
using namespace net::experimental::awaitable_operators;
using resolver = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::resolver>;
using timer_type = net::use_awaitable_t<>::as_default_on_t<net::steady_timer>;

using aedis::resp3::request;
using aedis::adapt;

// To avoid verbosity.
auto redir(boost::system::error_code& ec)
{
   return net::redirect_error(net::use_awaitable, ec);
}

void log(char const* msg, boost::system::error_code const& ec)
{
   std::clog << msg << ec.message() << std::endl;
}

bool
connect_condition(
    boost::system::error_code const& ec,
    auto const& next)
{
   return true;
}

auto reconnect(std::shared_ptr<connection> conn, request req) -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;
   timer_type timer{ex};
   resolver resv{ex};

   for (;;) {
      boost::system::error_code ec1, ec2;

      timer.expires_after(std::chrono::seconds{10});
      auto const addrs = co_await (
         resv.async_resolve("127.0.0.1", "6379") ||
         timer.async_wait()
      );

      timer.expires_after(std::chrono::seconds{5});
      co_await (
         conn->next_layer().async_connect(*std::get<0>(addrs).begin(), redir(ec1)) ||
         timer.async_wait()
      );

      log("async_connect: ", ec1);

      co_await (conn->async_run(redir(ec1)) && conn->async_exec(req, adapt(), redir(ec2)));

      log("async_run: ", ec1);
      log("async_exec: ", ec2);

      conn->reset_stream();
      timer.expires_after(std::chrono::seconds{1});
      co_await timer.async_wait();
   }
}

#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
