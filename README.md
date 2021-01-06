# Aedis

Aedis is a low level redis client designed for scalability and to
provide an easy and intuitive interface. Some of the supported
features are

* TLS, RESP3 and STL containers
* Pipelines (essential for performance).
* Coroutines, futures and callbacks.

At the moment the biggest missing parts is the Attribute data type as
its specification seems to be incomplete and I found no way to test
them.

## Tutorial

Sending a command to a redis server is as simple as

```cpp
   resp::request req;
   req.hello();
   req.set("Password", {"12345"});
   req.get("Password");
   req.quit();

   co_await resp::async_write(socket, req);
```

where `socket` is a tcp socket. Whereas reading looks like the
following
```cpp
   resp::response_set<int> res;
   co_await resp::async_read(socket, buffer, res);
```

Where the response type above depends on which command is being
expected.

A complete synchronous example can be seem bellow

```cpp
int main()
{
   try {
      resp::request req;
      req.hello();
      req.set("Password", {"12345"});
      req.get("Password");
      req.quit();

      net::io_context ioc {1};
      tcp::resolver resv(ioc);
      tcp::socket socket {ioc};
      net::connect(socket, resv.resolve("127.0.0.1", "6379"));
      resp::write(socket, req);

      std::string buffer;
      for (;;) {
	 switch (req.events.front().first) {
	    case resp::command::hello:
	    {
	       resp::response_flat_map<std::string> res;
	       resp::read(socket, buffer, res);
	    } break;
	    case resp::command::get:
	    {
	       resp::response_blob_string res;
	       resp::read(socket, buffer, res);
	    } break;
	    default:
	    {
	       resp::response_ignore res;
	       resp::read(socket, buffer, res);
	    }
	 }
	 req.events.pop();
      }
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
```
The important things to notice above are

* After connecting RESP3 requires the `hello` comand to be sent.
* Many commands are sent in the same request, the so called pipeline.
* Keep reading from the socket until it is closed by the redis server
  as requested by `quit`.
* The response is parsed in an appropriate buffer.

It is trivial to rewrite the example above to use coroutines, see
`examples/async_basic.cpp`. From now on we will use themas this is how
most people will be writting async servers in C++.

### Response buffer

To communicate efficiently with redis it is necessary to understand
the possible response types. RESP3 spcifies the following data types
`simple string`, `Simple error`, `number`, `double`, `bool`, `big
number`, `null`, `blob error`, `verbatim string`, `blob string`,
`streamed string part`.

These data types may come in different aggregate types `array`,
`push`, `set`, `map`, `attribute`. Aedis provides appropriate response
types for each of them.

### Events

The request type used above keeps a `std::queue` of commands in the
order they are expected to arrive. In addition to that you can
specify your own events

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

## Reconnecting and Sentinel support

In production we usually need a way to reconnect to the redis server
after a disconnect, some of the reasons are

1. The server has crashed and has been restarted by systemd.
1. All connection have been killed by the admin.
1. A failover operation has started.

It is easy to implement such a mechnism in scalable way using
coroutines, for example

```cpp
net::awaitable<void> example1()
{
   auto ex = co_await this_coro::executor;
   for (;;) {
      try {
	 resp::request req;
	 req.quit();

	 tcp::resolver resv(ex);
	 auto const r = resv.resolve("127.0.0.1", "6379");
	 tcp_socket socket {ex};
	 co_await async_connect(socket, r);
	 co_await async_write(socket, req);

	 std::string buffer;
	 for (;;) {
	    resp::response_ignore res;
	    co_await resp::async_read(socket, buffer, res);
	 }
      } catch (std::exception const& e) {
	 std::cerr << "Trying to reconnect ..." << std::endl;
	 stimer timer(ex, std::chrono::seconds{2});
	 co_await timer.async_wait();
      }
   }
}
```

More sophisticated reconnect strategies using sentinel are also easy
to implement using coroutines.

