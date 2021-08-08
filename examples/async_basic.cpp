/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/aedis.hpp>
#include <aedis/detail/utils.hpp>

using namespace aedis;

void f(request& req)
{
   req.ping();
   req.psubscribe({"aaa*"});
   req.quit();
}

class myreceiver : public receiver_base {
private:
   std::shared_ptr<connection> conn_;

public:
   myreceiver(std::shared_ptr<connection> conn) : conn_{conn} { }

   void on_hello(array_type& v) noexcept override
      { conn_->send(f); }

   void on_ping(simple_string_type& s) noexcept override
      { std::cout << "PING: " << s << std::endl; }

   void on_quit(simple_string_type& s) noexcept override
      { std::cout << "QUIT: " << s << std::endl; }

   void on_push(array_type& s) noexcept override
      { std::cout << "on_push: "; print(s); }
};

int main()
{
   net::io_context ioc {1};
   auto conn = std::make_shared<connection>(ioc.get_executor());
   myreceiver recv{conn};
   conn->start(recv);
   ioc.run();
}
