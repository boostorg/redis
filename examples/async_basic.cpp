/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/aedis.hpp>

using namespace aedis;

net::awaitable<void>
example(net::ip::tcp::socket& socket, std::queue<pipeline>& pipelines)
{
   pipelines.push({});
   pipelines.back().hello("3");

   std::string buffer;
   response_buffers buffers;
   response_adapters adapters{buffers};
   consumer_state cs;

   for (;;) {
      auto const type =
        co_await async_consume(
            socket, buffer, pipelines, adapters, cs, net::use_awaitable);

      if (type == resp3::type::push) {
         std::cout << "Event: " << "(" << type << ")" << std::endl;
         continue;
      }

      auto const cmd = pipelines.front().commands.front();

      std::cout << "Event: " << cmd << " (" << type << ")" << std::endl;
      switch (cmd) {
         case command::hello:
         {
            prepare_queue(pipelines);
            pipelines.back().ping();
            pipelines.back().subscribe("some-channel");
         } break;
         case command::publish: break;
         case command::quit: break;
         case command::ping:
         {
            prepare_queue(pipelines);
            pipelines.back().publish("some-channel", "Some message");
            pipelines.back().quit();
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

   std::queue<pipeline> pipelines;
   co_spawn(ioc, example(socket, pipelines), net::detached);
   ioc.run();
}
