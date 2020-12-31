# Aedis

Aedis is a low level redis client designed for scalability and to
provide an easy and intuitive interface. Some of the supported
features are

* RESP3: The new redis protocol.
* STL containers.
* Command pipelines (essential for performance).
* TLS.
* Coroutines, futures and callbacks.

At the moment the biggest missing parts are

* Attribute data type: Its specification is incomplete in my opinion
  and I found no meaningful way to test them as Redis itself doesn't
  seem to be usign them.
* Push type: I still did not manage to generate the notifications so I
  can test my implementation.

## Tutorial

A simple example is enough to show many of the aedis features 

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

* The `hello` command is included in the request as required by RESP3.
* Many commands are sent in the same request, the so called pipeline.
* We keep reading from the socket until it is closed by the redis
  server as requested in the quit command.
* The response is parsed in an appropriate buffer. The `hello` command in a map and
  the get into a string.

It is trivial to rewrite the example above to use coroutines, see
`examples/async_basic.cpp`. From now on we will use coroutines in the
tutorial as this is how most people should communicating to the redis
server usually.

### Response buffer

To communicate efficiently with redis it is necessary to understand
the possible response types. RESP3 spcifies the following data types

1. Simple string
1. Simple error
1. Number
1. Double
1. Bool
1. Big number
1. Null
1. Blob error
1. Verbatim string
1. Blob string
1. Streamed string part

These data types can come in different aggregate types

1. Array
1. Push
1. Set
1. Map
1. Attribute

Aedis provides appropriate response types for each of them.

### Events

The request type used above keeps a `std::queue` of commands in the
order they are expected to arrive. In addition to that you can
specify your own events

```cpp
enum class myevents
{ ignore
, list
, set
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

