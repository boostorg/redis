/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

auto reconnect(std::shared_ptr<connection> conn, request req) -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;
   stimer timer{ex};
   resolver resv{ex};

   for (;;) {
      boost::system::error_code ec1, ec2;

      // TODO: Run with a timer.
      auto const endpoints =
         co_await resv.async_resolve("127.0.0.1", "6379", net::redirect_error(net::use_awaitable, ec1));

      std::clog << "async_resolve: " << ec1.message() << std::endl;

      // TODO: Run with a timer.
      // TODO: Why default token is not working.
      //co_await net::async_connect(conn->next_layer(), endpoints, net::redirect_error(net::use_awaitable, ec1));
      co_await net::async_connect(conn->next_layer(), endpoints);

      std::clog << "async_connect: " << ec1.message() << std::endl;

      co_await (
         conn->async_run(net::redirect_error(net::use_awaitable, ec1)) &&
         conn->async_exec(req, adapt(), net::redirect_error(net::use_awaitable, ec2))
      );

      std::clog
         << "async_run: " << ec1.message() << "\n"
         << "async_exec: " << ec2.message() << std::endl;

      conn->reset_stream();
      timer.expires_after(std::chrono::seconds{1});
      co_await timer.async_wait();
   }
}
