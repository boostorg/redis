/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <boost/asio.hpp>
#include <aedis/aedis.hpp>

namespace net = aedis::net;
using namespace aedis;
using tcp = net::ip::tcp;

enum class myevents
{ one
, two
, three
, ignore
};

void fill(resp::request<myevents>& req)
{
   req.ping(myevents::one);
   req.rpush("list", {1, 2, 3});
   req.lrange("list");
   req.ping(myevents::two);
}

class myreceiver : public receiver_base<myevents> {
private:
   std::shared_ptr<connection<myevents>> conn_;

public:
   using event_type = myevents;

   myreceiver(std::shared_ptr<connection<myevents>> conn)
   : conn_{conn}
   { }

   void on_hello(myevents ev, resp::response_array::data_type& v) noexcept override
   {
      resp::print(v);
      conn_->send(fill);
   }
};

int main()
{
   net::io_context ioc {1};

   tcp::resolver resolver{ioc};
   auto const results = resolver.resolve("127.0.0.1", "6379");

   auto conn = std::make_shared<connection<myevents>>(ioc);
   myreceiver recv{conn};

   conn->start(recv, results);
   ioc.run();
}

