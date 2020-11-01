#include <boost/asio.hpp>

#include "aedis.hpp"

namespace net = aedis::net;
using tcp = net::ip::tcp;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<tcp::socket>;

namespace this_coro = net::this_coro;

using namespace net;
using namespace aedis;

awaitable<void> example()
{
   auto executor = co_await this_coro::executor;

   tcp::resolver resolver_(executor);
   auto const res = resolver_.resolve("127.0.0.1", "6379");

   tcp_socket socket {executor};
   co_await async_connect(socket, res);

   auto cmd = ping()
            + role()
            + multi()
            + set("age", {"39"})
            + incr("age")
            + get("age")
            + expire("Age", 10)
            + publish("channel", "message")
            + exec()
	    + quit()
	    ;

   co_await async_write(socket, buffer(cmd));

   resp::buffer buffer;
   for (;;) {
      co_await resp::async_read(socket, &buffer);
      resp::print(buffer.res);
      buffer.res.clear();
   }
}

int main()
{
   io_context ioc {1};
   co_spawn(ioc, example(), detached);
   ioc.run();
}
