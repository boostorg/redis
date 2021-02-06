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

enum class myevent {zero, one, two, ignore};

void fill1(resp::request<resp::event>& req)
{
   req.ping();
   req.rpush("list", {1, 2, 3});
   req.multi();
   req.lrange("list");
   req.exec();
   req.ping();
}

int main()
{
   net::io_context ioc {1};
   auto conn = std::make_shared<resp::connection<resp::event>>(ioc);
   resp::receiver_base<resp::event> recv;
   conn->start(recv);
   conn->send(fill1);
   ioc.run();
}

