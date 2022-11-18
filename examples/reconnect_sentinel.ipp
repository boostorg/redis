
using timer_type = net::use_awaitable_t<>::as_default_on_t<net::steady_timer>;

struct endpoint {
   std::string host;
   std::string port;
};

auto is_valid(endpoint const& ep) noexcept -> bool
{
   return !std::empty(ep.host) && !std::empty(ep.port);
}

net::awaitable<endpoint> resolve()
{
   // A list of sentinel addresses from which only one is responsive
   // to simulate sentinels that are down.
   std::vector<endpoint> const endpoints
   { {"foo", "26379"}
   , {"bar", "26379"}
   , {"127.0.0.1", "26379"}
   };

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
         co_await resv.async_resolve(ep.host, ep.port, net::redirect_error(net::use_awaitable, ec1));

      std::clog << "async_resolve: " << ec1.message() << std::endl;

      // TODO: Run with a timer.
      // TODO: Why default token is not working.
      //co_await net::async_connect(conn->next_layer(), endpoints, net::redirect_error(net::use_awaitable, ec1));
      co_await net::async_connect(conn.next_layer(), endpoints);

      co_await (
         conn.async_run(net::redirect_error(net::use_awaitable, ec1)) &&
         conn.async_exec(req, adapt(addr), net::redirect_error(net::use_awaitable, ec2))
      );

      std::clog << "async_run: " << ec1.message() << "\n"
                << "async_exec: " << ec2.message() << std::endl;

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

// See
// - https://redis.io/docs/manual/sentinel.
// - https://redis.io/docs/reference/sentinel-clients.
net::awaitable<void> reconnect(std::shared_ptr<connection> conn)
{
   request req;
   req.get_config().cancel_on_connection_lost = true;
   req.push("HELLO", 3);
   req.push("SUBSCRIBE", "channel");

   auto ex = co_await net::this_coro::executor;
   timer_type timer{ex};
   resolver resv{ex};
   for (;;) {
      auto ep = co_await net::co_spawn(ex, resolve(), net::use_awaitable);
      if (!is_valid(ep)) {
         std::clog << "Can't resolve master name" << std::endl;
         co_return;
      }

      boost::system::error_code ec1, ec2;

      // TODO: Run with a timer.
      auto const endpoints =
         co_await resv.async_resolve(ep.host, ep.port, net::redirect_error(net::use_awaitable, ec1));

      std::clog << "async_resolve: " << ec1.message() << std::endl;

      // TODO: Run with a timer.
      // TODO: Why default token is not working.
      //co_await net::async_connect(conn->next_layer(), endpoints, net::redirect_error(net::use_awaitable, ec1));
      co_await net::async_connect(conn->next_layer(), endpoints);

      co_await (
         conn->async_run(net::redirect_error(net::use_awaitable, ec1)) &&
         conn->async_exec(req, adapt(), net::redirect_error(net::use_awaitable, ec2))
      );

      std::clog << "async_run: " << ec1.message() << "\n"
                << "async_exec: " << ec2.message() << "\n"
                << "Starting the failover." << std::endl;

      timer.expires_after(std::chrono::seconds{1});
      co_await timer.async_wait();
   }
}

