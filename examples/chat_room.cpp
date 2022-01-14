#include <iostream>

#include <aedis/src.hpp>
#include <aedis/aedis.hpp>

#include "lib/client_base.hpp"
#include "lib/user_session.hpp"
#include "src.hpp"

using aedis::resp3::client_base;
using aedis::command;
using aedis::user_session;

namespace net = aedis::net;
using aedis::resp3::client_base;
using tcp_acceptor = aedis::net::use_awaitable_t<>::as_default_on_t<aedis::net::ip::tcp::acceptor>;
using client_base_type = client_base<aedis::resp3::response_id>;

class my_redis_client : public client_base_type {
private:
   void on_event(aedis::resp3::response_id id) override
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

struct on_message {
   std::shared_ptr<std::string> resp;
   std::shared_ptr<my_redis_client> client;
   std::shared_ptr<user_session> session;

   void operator()(std::string const& msg) const
   {
      auto filler = [this, &msg](auto& req)
	 { req.push(aedis::resp3::response_id{command::ping, resp, session}, msg); };

      client->send(filler);
   }
};

net::awaitable<void> listener()
{
   auto ex = co_await net::this_coro::executor;
   tcp_acceptor acceptor(ex, {net::ip::tcp::v4(), 55555});
   
   // The redis client instance.
   auto client = std::make_shared<my_redis_client>(ex);
   client->start();

   // The response is shared by all connections.
   auto resp = std::make_shared<std::string>();

   // Loops accepting connections.
   for (;;) {
      auto socket = co_await acceptor.async_accept();
      auto session = std::make_shared<user_session>(std::move(socket));
      session->start(on_message{resp, client, session});
   }
}

int main()
{
  try {
    net::io_context io_context(1);
    net::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&](auto, auto){ io_context.stop(); });
    co_spawn(io_context, listener(), net::detached);
    io_context.run();
  } catch (std::exception& e) {
     std::cerr << e.what() << std::endl;
  }
}
