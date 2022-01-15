/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>
#include <unordered_map>
#include <vector>

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
   // TODO: Not needed here. Fix client_base and remove this.
   std::shared_ptr<std::string> resp;
};

using client_base_type = client_base<response_id>;

class my_redis_client : public client_base_type {
private:
   std::vector<std::weak_ptr<user_session_base>> sessions_;

   void on_message(response_id id) override
   {
      id.resp->clear();
   }

   void on_push() override
   {
      for (auto& weak: sessions_) {
	 if (auto session = weak.lock()) {
	    session->deliver(push_resp_.at(3).data);
	 } else {
	    std::cout << "Session expired." << std::endl;
	 }
      }

      push_resp_.clear();
   }

public:
   my_redis_client(net::any_io_executor ex)
   : client_base_type(ex)
   {}

   auto subscribe(std::shared_ptr<user_session_base> session)
      { sessions_.push_back(session); }
};

struct on_user_msg {
   std::shared_ptr<std::string> resp;
   std::shared_ptr<my_redis_client> client;

   void operator()(std::string const& msg)
   {
      auto filler = [this, &msg](auto& req)
      {
	 response_id id{command::publish, resp};
	 req.push(id, "channel", msg);
      };

      client->send(filler);
   }
};

net::awaitable<void> listener()
{
   auto ex = co_await net::this_coro::executor;
   net::ip::tcp::acceptor acceptor(ex, {net::ip::tcp::v4(), 55555});

   // The response is shared by all connections.
   auto resp = std::make_shared<std::string>();
   
   // The redis client instance.
   auto client = std::make_shared<my_redis_client>(ex);
   client->start();

   auto filler = [resp](auto& req)
      { req.push(response_id{command::subscribe, resp}, "channel"); };

   client->send(filler);

   for (;;) {
      auto socket = co_await acceptor.async_accept(net::use_awaitable);
      auto session = std::make_shared<user_session>(std::move(socket));
      client->subscribe(session);
      session->start(on_user_msg{resp, client});
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
