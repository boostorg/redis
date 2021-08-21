/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/connection.hpp>
#include <aedis/detail/read.hpp>
#include <aedis/detail/write.hpp>

namespace aedis {

connection::connection(net::any_io_executor const& ioc, config const& conf)
: socket_{ioc}
, conf_{conf}
{
}

void connection::start(receiver_base& recv, buffers& bufs)
{
   auto self = this->shared_from_this();

   auto receiver = [&](auto cmd, auto type)
   {
     switch (type) {
       case resp3::type::push: recv.on_push(); break;
       default: detail::forward(cmd, type, recv);
     }
   };

   auto f = [self, receiver, &bufs] () mutable
     { return self->worker_coro(receiver, bufs); };

   net::co_spawn(socket_.get_executor(), f, net::detached);
}

} // aedis
