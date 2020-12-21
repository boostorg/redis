# Aedis

Aedis is a low level redis client designed for scalability while
providing and to provide an easy and intuitive interface.

## Tutorial

We begin with sync events below and jump to async code thereafter.

### Sync

Synchronous code are clear by themselves

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
	 resp::response_string res;
	 resp::read(socket, buffer, res);
	 std::cout << res.result << std::endl;
      }
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
```

We keep reading from the socket until it is closed as a result of the
quit command. The commands are sent in a pipeline to redis, which
greatly improves performance. Notice also we parse the result in the
buffer resp::reponse_string. This is overly simplistic for most apps.
Typically we will want to parse each response in an appropriate
data structure.

#### Response buffer

Aedis comes with general purpose response buffers that are suitable to
parse directly in C++ built-in data types and containers or on your
own. For example

```cpp
int main()
{
   try {
      ... // Like before
      std::string buffer;

      resp::response_int<int> list_size; // Parses into an int
      resp::read(socket, buffer, list_size);
      std::cout << list_size.result << std::endl;

      resp::response_list<int> list; // Parses into a std::list<int>
      resp::read(socket, buffer, list);
      print(list.result);

      resp::response_string ok; // Parses into a std::string
      resp::read(socket, buffer, ok);
      std::cout << ok.result << std::endl;

      resp::response noop;
      resp::read(socket, buffer, noop);

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
```

Other response types are available also. Structure the code like in
the example above is usually not feasible as the commands send are
determined dynamically, for that case we have events.

#### Events

Most times, that events that are added to the pipeline are decided
dynamically and we are not interested in the response for some of
them. To deal with this we support events. To use them, define an
enum class like the one below

```cpp
enum class myevents
{ ignore
, list
, set
};
```

Pass it as argument to the pipeline class as below and use the
appropriate enum field as the last argument to every command in the
pipeline you want to use the result

```cpp
int main()
{
   try {
      resp::pipeline<myevents> p;
      p.rpush("list", {1, 2, 3});
      p.lrange("list", 0, -1, myevents::list);
      p.sadd("set", std::set<int>{3, 4, 5});
      p.smembers("set", myevents::set);
      p.quit();

      io_context ioc {1};
      tcp::resolver resv(ioc);
      tcp::socket socket {ioc};
      net::connect(socket, resv.resolve("127.0.0.1", "6379"));
      net::write(socket, buffer(p.payload));

      std::string buffer;
      for (;;) {
	 switch (p.events.front()) {
	 case myevents::list:
	 {
	    resp::response_list<int> res;
	    resp::read(socket, buffer, res);
	    print(res.result);
	 } break;
	 case myevents::set:
	 {
	    resp::response_set<int> res;
	    resp::read(socket, buffer, res);
	    print(res.result);
	 } break;
	 default:
	 {
	    resp::response res;
	    resp::read(socket, buffer, res);
	 }
	 }
	 p.events.pop();
      }
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
```

### Async

The sync examples above are good as introduction and can be also
useful in production. However, sync code doesn't scale well, this is
specially problematic on backends. Fortunately in C++20 it became
trivial to convert sync into asyn code. The example below shows
the async version of our first sync example.

```cpp
net::awaitable<void> example1()
{
   auto ex = co_await this_coro::executor;

   tcp::resolver resv(ex);
   auto const r = resv.resolve("127.0.0.1", "6379");

   tcp_socket socket {ex};
   co_await async_connect(socket, r);

   resp::pipeline p;
   p.set("Password", {"12345"});
   p.quit();

   co_await async_write(socket, buffer(p.payload));

   std::string buffer;
   for (;;) {
      resp::response_string res;
      co_await resp::async_read(socket, buffer, res);
      std::cout << res.result << std::endl;
   }
}
```

Basically, we only have to replace read with async_read and use the
co_await keyword.

## Reconnecting and Sentinel support

On some occasions we will want to reconnet to redis server after a
disconnect. This can happen for a couple of reasons, for example, the
redis-server may crash and be restarted by systemd.

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

