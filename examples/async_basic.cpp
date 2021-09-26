/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/aedis.hpp>

using namespace aedis;

void print_event(resp3::type t, std::pair<command, std::string> const& p)
{
   std::cout << "Event: " << p.first << ".";

   if (!std::empty(p.second))
      std::cout << " Key: " << p.second << ".";

   std::cout << " Type: " << t << std::endl;
}

net::awaitable<void>
example(net::ip::tcp::socket& socket,
        std::queue<resp3::request>& requests)
{
   requests.push({});
   requests.back().hello("3");

   resp3::response resp;
   resp3::consumer cs;

   for (;;) {
      auto const t = co_await cs.async_consume(socket, requests, resp, net::use_awaitable);

      if (t == resp3::type::flat_push) {
         std::cout << "Event: " << "(" << t << ")" << std::endl;
         continue;
      }

      auto const id = requests.front().ids.front();

      print_event(t, id);
      switch (id.first) {
         case command::hello:
         {
            prepare_next(requests);
            requests.back().ping();
            requests.back().subscribe("some-channel");
         } break;
         case command::publish: break;
         case command::quit: break;
         case command::ping:
         {
            prepare_next(requests);
            requests.back().publish("some-channel", "Some message");
            requests.back().quit();
         } break;
         default: { }
      }
   }
}

int main()
{
   net::io_context ioc;
   net::ip::tcp::resolver resolver{ioc};
   auto const res = resolver.resolve("127.0.0.1", "6379");

   net::ip::tcp::socket socket{ioc};
   net::connect(socket, res);

   std::queue<resp3::request> requests;
   co_spawn(ioc, example(socket, requests), net::detached);
   ioc.run();
}
