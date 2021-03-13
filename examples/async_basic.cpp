/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/aedis.hpp>

using namespace aedis;

void f(request& req)
{
   req.ping();
   req.quit();
}

class receiver : public receiver_base {
private:
   std::shared_ptr<connection> conn_;

public:
   receiver(std::shared_ptr<connection> conn) : conn_{conn} { }

   void on_hello(resp::array_type& v) noexcept override
      { conn_->send(f); }

   void on_ping(resp::simple_string_type& s) noexcept override
      { std::cout << "PING: " << s << std::endl; }

   void on_quit(resp::simple_string_type& s) noexcept override
      { std::cout << "QUIT: " << s << std::endl; }
};

int main()
{
   net::io_context ioc {1};
   net::ip::tcp::resolver resolver{ioc};
   auto const results = resolver.resolve("127.0.0.1", "6379");
   auto conn = std::make_shared<connection>(ioc);
   receiver recv{conn};
   conn->start(recv, results);
   ioc.run();
}
