/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>

#include <aedis/src.hpp>
#include <aedis/aedis.hpp>

#include "lib/client_base.hpp"
#include "lib/user_session.hpp"
#include "src.hpp"

using aedis::resp3::client_base;
using aedis::command;
using aedis::user_session;
using aedis::user_session_base;

namespace net = aedis::net;
using aedis::resp3::client_base;

// Holds the information that is needed when a response to a
// request arrives. See client_base.hpp for more details on the
// required fields in this struct.
struct response_id {
   // The redis command that corresponds to this command. 
   command cmd = command::unknown;

   // Pointer to the response.
   std::shared_ptr<std::string> resp;

   // The pointer to the session the request belongs to.
   std::weak_ptr<user_session_base> session =
      std::shared_ptr<user_session_base>{nullptr};
};

using client_base_type = client_base<response_id>;

class my_redis_client : public client_base_type {
private:
   void on_message(response_id id) override
   {
      // If the user connections is still alive when the response
      // arrives we send the echo message to the user, otherwise we
      // just log it has expired.
      if (auto session = id.session.lock()) {
         session->deliver(*id.resp);
	 id.resp->clear();
      } else {
         std::cout << "Session expired." << std::endl;
      }
   }

public:
   my_redis_client(net::any_io_executor ex)
   : client_base_type(ex)
   {}
};

struct on_user_msg {
   std::shared_ptr<std::string> resp;
   std::shared_ptr<my_redis_client> client;
   std::shared_ptr<user_session> session;

   void operator()(std::string const& msg) const
   {
      auto filler = [this, &msg](auto& req)
	 { req.push(response_id{command::ping, resp, session}, msg); };

      client->send(filler);
   }
};

net::awaitable<void> listener()
{
   auto ex = co_await net::this_coro::executor;
   net::ip::tcp::acceptor acceptor(ex, {net::ip::tcp::v4(), 55555});
   
   // The redis client instance.
   auto client = std::make_shared<my_redis_client>(ex);
   client->start();

   // The response is shared by all connections.
   auto resp = std::make_shared<std::string>();

   // Loops accepting connections.
   for (;;) {
      auto socket = co_await acceptor.async_accept(net::use_awaitable);
      auto session = std::make_shared<user_session>(std::move(socket));
      session->start(on_user_msg{resp, client, session});
   }
}

int main()
{
   try {
      net::io_context ioc{1};
      net::signal_set signals(ioc, SIGINT, SIGTERM);
      signals.async_wait([&](auto, auto){ ioc.stop(); });
      co_spawn(ioc, listener(), net::detached);
      ioc.run();
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
