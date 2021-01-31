/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <boost/asio.hpp>
#include <aedis/aedis.hpp>
#include <aedis/receiver_print.hpp>

#include <stack>

namespace net = aedis::net;
using namespace aedis;
using tcp = net::ip::tcp;

void fill1(resp::request<resp::event>& req)
{
   req.ping();
   //req.multi();
   req.rpush("list", {1, 2, 3});
   req.lrange("list");
   //req.exec();
   req.ping();
}

net::awaitable<void> example()
{
   try {
      auto ex = co_await net::this_coro::executor;
      tcp::resolver resv(ex);
      auto const r = resv.resolve("127.0.0.1", "6379");
      tcp::socket socket {ex};
      co_await async_connect(socket, r, net::use_awaitable);

      auto reqs = resp::make_request_queue<resp::event>();
      resp::response_buffers resps;
      resp::receiver_print recv{resps};
      net::steady_timer st{ex};

      co_spawn(ex, resp::async_reader(socket, reqs, resps, recv), net::detached);
      resp::async_writer(socket, reqs, st, net::detached);
      queue_writer(reqs, fill1, st);

      net::steady_timer timer(ex, std::chrono::years{1});
      co_await timer.async_wait(net::use_awaitable);

   } catch (std::exception const& e) {
      std::cout << e.what() << std::endl;
   }
}

int main()
{
   net::io_context ioc {1};
   co_spawn(ioc, example(), net::detached);
   ioc.run();
}


