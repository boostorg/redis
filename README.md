# Aedis

Aedis is a low level redis client designed for scalability and to
provide an easy and intuitive interface. All protocol features are
supported (to the best of my knowledge), some of them are

* Command pipelines (essential for performance).
* TLS: Automatically supported since aedis uses ASIO's `AsyncReadStream`.
* ASIO asynchronous model where futures, callbacks and coroutines are
  supported.

## Tutorial

Let us begin with a synchronous example 

```cpp
int main()
{
   try {
      resp::pipeline p;
      p.set("Password", {"12345"});
      p.quit();

      io_context ioc {1};
      tcp::resolver resv(ioc);
      tcp::socket socket {ioc};
      net::connect(socket, resv.resolve("127.0.0.1", "6379"));
      net::write(socket, buffer(p.payload));

      std::string buffer;
      for (;;) {
	 resp::response_simple_string res;
	 resp::read(socket, buffer, res);
	 std::cout << res.result << std::endl;
      }
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
```

The important things to notice above are

* We keep reading from the socket until it is closed by the redis
  server (as requested by the quit command).

* The commands are composed with the `pipeline` class.

* The response is parsed in an appropriate buffer `response_simple_string`.

Converting the example above to use coroutines is trivial

```cpp
net::awaitable<void> example1()
{
   resp::pipeline p;
   p.set("Password", {"12345"});
   p.quit();

   auto ex = co_await this_coro::executor;
   tcp::resolver resv(ex);
   auto const r = resv.resolve("127.0.0.1", "6379");
   tcp_socket socket {ex};
   co_await async_connect(socket, r);
   co_await async_write(socket, buffer(p.payload));

   std::string buffer;
   for (;;) {
      resp::response_simple_string res;
      co_await resp::async_read(socket, buffer, res);
      std::cout << res.result << std::endl;
   }
}
```

From now on we will use coroutines in the tutorial as this is how most
people should communicating to the redis server usually.

### Response buffer

To communicate efficiently with redis it is necessary to understand
the possible response types. RESP3 spcifies the following data types

1. simple string
1. simple error
1. number
1. double
1. bool
1. big number
1. null
1. blob error
1. verbatim string
1. blob string
1. streamed string part

These data types can come in different aggregate types

1. array
1. push
1. set
1. map
1. attribute

Aedis provides appropriate response types for each of the data and
aggregate types. For example

```cpp
net::awaitable<void> example()
{
   try {
      resp::pipeline p;
      p.rpush("list", {1, 2, 3});
      p.lrange("list");
      p.quit();

      auto ex = co_await this_coro::executor;
      tcp::resolver resv(ex);
      tcp_socket socket {ex};
      co_await net::async_connect(socket, resv.resolve("127.0.0.1", "6379"));
      co_await net::async_write(socket, net::buffer(p.payload));

      std::string buffer;
      resp::response_number<int> list_size;
      co_await resp::async_read(socket, buffer, list_size);

      resp::response_list<int> list;
      co_await resp::async_read(socket, buffer, list);

      resp::response_simple_string ok;
      co_await resp::async_read(socket, buffer, ok);

      resp::response noop;
      co_await resp::async_read(socket, buffer, noop);

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
```

Usually the commands that are sent to redis are determined dynamically
so it is not possible to structure the code like above, to deal with
that it we support events.

#### Events

To use events, define an enum class like the one below

```cpp
enum class myevents
{ ignore
, list
, set
};
```

and pass it as argument to the pipeline commands as below

```cpp
net::awaitable<void> example()
{
   try {
      resp::pipeline<myevents> p;
      p.rpush("list", {1, 2, 3});
      p.lrange("list", 0, -1, myevents::list);
      p.sadd("set", std::set<int>{3, 4, 5});
      p.smembers("set", myevents::set);
      p.quit();

      auto ex = co_await this_coro::executor;
      tcp::resolver resv(ex);
      tcp_socket socket {ex};
      co_await net::async_connect(socket, resv.resolve("127.0.0.1", "6379"));
      co_await net::async_write(socket, buffer(p.payload));

      std::string buffer;
      for (;;) {
	 switch (p.events.front()) {
	 case myevents::list:
	 {
	    resp::response_list<int> res;
	    co_await resp::async_read(socket, buffer, res);
	    print(res.result);
	 } break;
	 case myevents::set:
	 {
	    resp::response_set<int> res;
	    co_await resp::async_read(socket, buffer, res);
	    print(res.result);
	 } break;
	 default:
	 {
	    resp::response res;
	    co_await resp::async_read(socket, buffer, res);
	 }
	 }
	 p.events.pop();
      }
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
```

## Reconnecting and Sentinel support

In production we need a reconnect mechanism, some of the reasons are

1. The server has crashed and has been restarted by systemd.
1. All connection have been killed by the admin.
1. A failover operation has started.

### Simple reconnet

It is trivial to implement a reconnect using a coroutine

```cpp
net::awaitable<void> example1()
{
   auto ex = co_await this_coro::executor;
   for (;;) {
      try {
	 resp::pipeline p;
	 p.set("Password", {"12345"});
	 p.quit();

	 tcp::resolver resv(ex);
	 auto const r = resv.resolve("127.0.0.1", "6379");
	 tcp_socket socket {ex};
	 co_await async_connect(socket, r);
	 co_await async_write(socket, buffer(p.payload));

	 std::string buffer;
	 for (;;) {
	    resp::response_string res;
	    co_await resp::async_read(socket, buffer, res);
	    std::cout << res.result << std::endl;
	 }
      } catch (std::exception const& e) {
	 std::cerr << "Error: " << e.what() << std::endl;
	 stimer timer(ex);
	 timer.expires_after(std::chrono::seconds{2});
	 co_await timer.async_wait();
      }
   }
}
```

For many usecases this is often enough. A more sophisticated reconnect
strategy however is to use a redis-sentinel.

### Sentinel

