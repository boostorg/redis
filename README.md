# Aedis

Aedis is a redis client designed for scalability and performance while
providing an easy and intuitive interface. Some of the supported
features are

* TLS, RESP3 and STL containers.
* Pipelines (essential for performance).
* Coroutines, futures and callbacks.

## Tutorial

All you have to do is to define an `enum` that defines the events you
want to handle, if any, and a receiver. For example

```cpp
   enum class events {one, two, three, ignore};

   void f(request<events>& req)
   {
      req.ping(events::one);
      req.quit();
   }

   class receiver : public receiver_base<events> {
   private:
      std::shared_ptr<connection<events>> conn_;

   public:
      using event_type = events;
      receiver(std::shared_ptr<connection<events>> conn) : conn_{conn} { }

      void on_hello(events ev, resp::array_type& v) noexcept override
	 { conn_->send(f); }

      void on_ping(events ev, resp::simple_string_type& s) noexcept override
	 { std::cout << "PING: " << s << std::endl; }

      void on_quit(events ev, resp::simple_string_type& s) noexcept override
	 { std::cout << "QUIT: " << s << std::endl; }
   };

```

Inheriting from the `receiver_base` class is not needed but convenient
to avoid writing the complete receiver interface. In general for each
redis command you have to override (or offer) a member function in the
receiver.

The main function looks like

```cpp
   int main()
   {
      net::io_context ioc {1};
      net::ip::tcp::resolver resolver{ioc};
      auto const results = resolver.resolve("127.0.0.1", "6379");
      auto conn = std::make_shared<connection<events>>(ioc);
      receiver recv{conn};
      conn->start(recv, results);
      ioc.run();
   }
```

## Low level API

A low level api is also offered, though I don't recommend using it as
is offers no real advantage over the high level as shown above.
Sending a command to a redis server is done as follows

```cpp
   resp::request req;
   req.hello();
   req.set("Password", {"12345"});
   req.get("Password");
   req.quit();

   co_await resp::async_write(socket, req);
```

where `socket` is a tcp socket. Reading on the other hand looks like
the following

```cpp
   resp::response_set<int> res;
   co_await resp::async_read(socket, buffer, res);
```

The response type above depends on which command is being expected.
This library also offers the synchronous functions to parse `RESP`, in
this tutorial however we will focus in async code.

A complete example can be seem bellow

```cpp
net::awaitable<void> example()
{
   try {
      auto ex = co_await this_coro::executor;

      resp::request p;
      p.hello();
      p.rpush("list", {1, 2, 3});
      p.lrange("list");
      p.quit();

      tcp::resolver resv(ex);
      tcp_socket socket {ex};
      co_await net::async_connect(socket, resv.resolve("127.0.0.1", "6379"));
      co_await net::async_write(socket, net::buffer(p.payload));

      std::string buffer;
      resp::response_flat_map<std::string> hello;
      co_await resp::async_read(socket, buffer, hello);

      resp::response_number<int> rpush;
      co_await resp::async_read(socket, buffer, rpush);
      std::cout << rpush.result << std::endl;

      resp::response_list<int> lrange;
      co_await resp::async_read(socket, buffer, lrange);
      print(lrange.result);

      resp::response_simple_string quit;
      co_await resp::async_read(socket, buffer, quit);
      std::cout << quit.result << std::endl;

      resp::response_ignore eof;
      co_await resp::async_read(socket, buffer, eof);

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
```

The important things to notice above are

* After connecting RESP3 requires the `hello` command to be sent.
* Many commands are sent in the same request, the so called pipeline.
* Keep reading from the socket until it is closed by the redis server
  as requested by `quit`.
* The response is parsed directly in a buffer that is suitable to the
  application.

### Response buffer

To communicate efficiently with redis it is necessary to understand
the possible response types. RESP3 specifies the following data types
`simple string`, `Simple error`, `number`, `double`, `bool`, `big
number`, `null`, `blob error`, `verbatim string`, `blob string`,
`streamed string part`.  These data types may come in different
aggregate types `array`, `push`, `set`, `map`, `attribute`. Aedis
provides appropriate response types for each of them.

### Events

The request type used above keeps a `std::queue` of commands in the
order they are expected to arrive. In addition to that you can specify
your own events

```cpp
enum class myevents
{ ignore
, interesting1
, interesting2
};
```

and pass it as argument to the request as follows

```cpp
net::awaitable<void> example()
{
   try {
      resp::request<myevents> req;
      req.rpush("list", {1, 2, 3});
      req.lrange("list", 0, -1, myevents::interesting1);
      req.sadd("set", std::set<int>{3, 4, 5});
      req.smembers("set", myevents::interesting2);
      req.quit();

      auto ex = co_await this_coro::executor;
      tcp::resolver resv(ex);
      tcp_socket socket {ex};
      co_await net::async_connect(socket, resv.resolve("127.0.0.1", "6379"));
      co_await resp::async_write(socket, req);

      std::string buffer;
      for (;;) {
	 switch (req.events.front().second) {
	    case myevents::interesting1:
	    {
	       resp::response_list<int> res;
	       co_await resp::async_read(socket, buffer, res);
	       print(res.result);
	    } break;
	    case myevents::interesting2:
	    {
	       resp::response_set<int> res;
	       co_await resp::async_read(socket, buffer, res);
	       print(res.result);
	    } break;
	    default:
	    {
	       resp::response_ignore res;
	       co_await resp::async_read(socket, buffer, res);
	    }
	 }
	 req.events.pop();
      }
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
```
