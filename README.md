# Aedis

Aedis is a redis client designed for scalability and performance while
providing an easy and intuitive interface.

## Example

Lets us see some examples with increasing order of complexity.
See the `examples` directory for more examples.

### Ping

A very simple example with illustrative purposes.  The example above
will start writing the `hello` command and proceed reading its
response. After that users can add further commands to the queue. See
the example directory for a complete example.


```cpp
net::awaitable<void> ping()
{
   auto socket = co_await make_connection();

   std::queue<resp3::request> requests;
   requests.push({});
   requests.back().hello();
   requests.back().ping();
   requests.back().quit();

   resp3::consumer cs;
   for (;;) {
      resp3::response resp;
      co_await cs.async_consume(socket, requests, resp);
      std::cout << requests.front().elements.front() << "\n" << resp << std::endl;
   }
}
```

### Pubsub

Publisher-subscriber in redis.

```cpp
net::awaitable<void> subscriber()
{
   auto ex = net::this_coro::executor;
   auto socket = co_await make_connection();

   std::string id;

   std::queue<resp3::request> requests;
   requests.push({});
   requests.back().hello();

   resp3::consumer cs;
   for (;;) {
      resp3::response resp;
      co_await cs.async_consume(socket, requests, resp);

      if (resp.get_type() == resp3::type::push) {
	 std::cout << "Subscriber " << id << ":\n" << resp << std::endl;
         continue;
      }

      if (requests.front().elements.front().cmd == command::hello) {
	 id = resp.raw().at(8).data;
	 prepare_next(requests);
	 requests.back().subscribe({"channel1", "channel2"});
      }
   }
}
```

The publisher looks like the following.

```cpp
net::awaitable<void> publisher()
{
   auto ex = net::this_coro::executor;
   auto socket = co_await make_connection();

   std::queue<resp3::request> requests;
   requests.push({});
   requests.back().hello();

   resp3::consumer cs;
   for (;;) {
      resp3::response resp;
      co_await cs.async_consume(socket, requests, resp);

      if (requests.front().elements.front().cmd == command::hello) {
	 prepare_next(requests);
	 requests.back().publish("channel1", "Message to channel1");
	 requests.back().publish("channel2", "Message to channel2");
	 requests.back().quit();
      }
   }
}
```

## Installation

This library is header only. To install it run

```cpp
$ sudo make install
```

or copy the include folder to where you want.  You will also need to include
the following header in one of your source files e.g. `aedis.cpp`

```cpp
#include <aedis/impl/src.hpp>
```
