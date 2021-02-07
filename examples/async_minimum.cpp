/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <boost/asio.hpp>
#include <aedis/aedis.hpp>

using namespace aedis;

/* This example shows the absolute minimum you need to stablish a
 * connection with redis.
 *
 *    1. Write an enum class that defines your events.
 *
 *    2. Write a receiver. The receiver_base class below is not
 *       required if your receiver class supports the receiver
 *       concept.
 *
 * In the next examples we will see how to receive and write commands.
 */

enum class events {one, two, three, ignore};
struct receiver : public receiver_base<events> { };

int main()
{
   net::io_context ioc {1};
   net::ip::tcp::resolver resolver{ioc};
   auto const results = resolver.resolve("127.0.0.1", "6379");
   auto conn = std::make_shared<connection<events>>(ioc);
   receiver recv;
   conn->start(recv, results);
   ioc.run();
}

