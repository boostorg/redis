/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>

#include <aedis/aedis.hpp>

#include "types.hpp"
#include "utils.ipp"
#include "client_base.hpp"

using aedis::command;
using aedis::resp3::request;
using aedis::resp3::type;
using aedis::resp3::response;
using aedis::resp3::response_base;
using aedis::resp3::client_base;

namespace net = aedis::net;

class myclient : public client_base {
private:
   void on_event() override
   {
      if (resp_.get_type() == type::push) {
	 std::cout << resp_ << std::endl;
      } else {
	 std::cout
	    << reqs_.front().commands.front() << ":\n"
	    << resp_ << std::endl;
      }

      resp_.clear();
   }

public:
   myclient(net::any_io_executor ex)
   : client_base(ex) { }
};

// A coroutine that will call the filler every second.
template <class Filler>
net::awaitable<void>
event_simulator(std::shared_ptr<myclient> rclient, Filler filler)
{
   auto ex = co_await net::this_coro::executor;
   net::steady_timer t{ex};

   for (;;) {
      t.expires_after(std::chrono::seconds{1});
      co_await t.async_wait(net::use_awaitable);
      rclient->send(filler);
   }
}

int main()
{
   net::io_context ioc;
   auto rclient = std::make_shared<myclient>(ioc.get_executor());
   rclient->start();

   auto filler = [](auto& req)
   {
     req.push(command::incr, "key");
     req.push(command::quit);
     req.push(command::incr, "key");
   };

   for (auto i = 0; i < 1; ++i)
      co_spawn(ioc.get_executor(), event_simulator(rclient, filler), net::detached);

   ioc.run();
}

/// \example intro5.cpp
