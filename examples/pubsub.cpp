/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>

#include <aedis/aedis.hpp>

#include "utils.ipp"

using namespace aedis;

net::awaitable<void> example()
{
   auto socket = co_await make_connection();

   std::queue<resp3::request> requests;
   requests.push({});
   requests.back().hello();

   resp3::consumer cs;
   for (;;) {
      resp3::response resp;
      co_await cs.async_consume(socket, requests, resp);
      std::cout << resp << std::endl;

      if (resp.get_type() == resp3::type::push)
         continue;

      auto const& elem = requests.front().elements.front();

      std::cout << elem << std::endl;
      switch (elem.cmd) {
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
   co_spawn(ioc, example(), net::detached);
   ioc.run();
}
