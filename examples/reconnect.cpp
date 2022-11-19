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

auto redir(boost::system::error_code& ec)
   { return net::redirect_error(net::use_awaitable, ec); }

void log(char const* msg)
   { std::clog << msg << std::endl; }

void log(char const* msg, boost::system::error_code const& ec)
   { std::clog << msg << ec.message() << std::endl; }

auto reconnect_simple(std::shared_ptr<connection> conn, request req) -> net::awaitable<void>
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

struct endpoint {
   std::string host;
   std::string port;
};

auto is_valid(endpoint const& ep) noexcept -> bool
{
   return !std::empty(ep.host) && !std::empty(ep.port);
}

auto resolve_master_address(std::vector<endpoint> const& endpoints) -> net::awaitable<endpoint>
{
   request req;
   req.get_config().cancel_on_connection_lost = true;
   req.push("SENTINEL", "get-master-addr-by-name", "mymaster");
   req.push("QUIT");

   auto ex = co_await net::this_coro::executor;
   connection conn{ex};

   std::tuple<std::optional<std::array<std::string, 2>>, aedis::ignore> addr;
   resolver resv{ex};
   for (auto ep : endpoints) {
      boost::system::error_code ec1, ec2;

      // TODO: Run with a timer.
      auto const endpoints =
         co_await resv.async_resolve(ep.host, ep.port, redir(ec1));

      log("async_resolve: ", ec1);

      // TODO: Run with a timer.
      // TODO: Why default token is not working.
      //co_await net::async_connect(conn->next_layer(), endpoints, redir(ec1));
      co_await net::async_connect(conn.next_layer(), endpoints);

      co_await (
         conn.async_run(redir(ec1)) &&
         conn.async_exec(req, adapt(addr), redir(ec2))
      );

      log("async_run: ", ec1);
      log("async_exec: ", ec2);

      conn.reset_stream();
      if (std::get<0>(addr))
         break;
   }

   endpoint ep;
   if (std::get<0>(addr)) {
      ep.host = std::get<0>(addr).value().at(0);
      ep.port = std::get<0>(addr).value().at(1);
   }

   co_return ep;
}

auto reconnect_sentinel(std::shared_ptr<connection> conn, aedis::resp3::request req) -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;
   timer_type timer{ex};
   resolver resv{ex};

   // A list of sentinel addresses from which only one is responsive
   // to simulate sentinels that are down.
   std::vector<endpoint> const endpoints
   { {"foo", "26379"}
   , {"bar", "26379"}
   , {"127.0.0.1", "26379"}
   };

   for (;;) {
      auto ep = co_await net::co_spawn(ex, resolve_master_address(endpoints), net::use_awaitable);
      if (!is_valid(ep)) {
         log("Can't resolve master name");
         co_return;
      }

      boost::system::error_code ec1, ec2;

      // TODO: Run with a timer.
      auto const endpoints =
         co_await resv.async_resolve(ep.host, ep.port, redir(ec1));

      log("async_resolve: ", ec1);

      // TODO: Run with a timer.
      // TODO: Why default token is not working.
      //co_await net::async_connect(conn->next_layer(), endpoints, redir(ec1));
      co_await net::async_connect(conn->next_layer(), endpoints);

      co_await (conn->async_run(redir(ec1)) && conn->async_exec(req, adapt(), redir(ec2)));

      log("async_run: ", ec1);
      log("async_exec: ", ec2);
      log("Starting the failover ...");

      timer.expires_after(std::chrono::seconds{1});
      co_await timer.async_wait();
   }
}

auto
reconnect(
   std::shared_ptr<connection> conn,
   request req,
   bool use_sentinel) -> net::awaitable<void>
{
   if (use_sentinel)
      return reconnect_sentinel(conn, req);
   else
      return reconnect_simple(conn, req);
}

#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
