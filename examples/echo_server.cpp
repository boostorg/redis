#include <iostream>

#include <aedis/src.hpp>
#include <aedis/aedis.hpp>
#include <aedis/resp3/client_base.hpp>
#include "user_session.hpp"

namespace net = aedis::net;
using aedis::resp3::client_base;
using tcp_acceptor = aedis::net::use_awaitable_t<>::as_default_on_t<aedis::net::ip::tcp::acceptor>;

class my_redis_client : public client_base_type {
private:
   void on_event(response_id id) override
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
      auto session = std::make_shared<user_session>(std::move(socket), client);

      response_id id{command::ping, resp, session};
      auto filler = [id](auto& req, auto const& msg)
	 { req.push(id, msg); };

      session->start(filler);
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
