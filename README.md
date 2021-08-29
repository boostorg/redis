# Aedis

Aedis is a redis client designed for scalability and performance while
providing an easy and intuitive interface.

## Example

The general form of the read and write operations of a redis client
that support push notifications and pipelines looks like the following

```cpp
net::awaitable<void> reader(net::ip::tcp::resolver::results_type const& res)
{
   auto ex = co_await net::this_coro::executor;

   net::ip::tcp::socket socket{ex};
   co_await net::async_connect(socket, res, net::use_awaitable);

   std::string read_buffer;
   response_buffers buffers;
   std::queue<pipeline> pipelines;

   pipelines.push({});
   pipelines.back().hello("3");

   for (;;) {
      co_await async_write_some(socket, pipelines);

      do {
	 do {
	    auto const event = co_await async_read_one(socket, read_buffer, buffers, pipelines);
	    if (event.second != resp3::type::push)
	       pipelines.front().commands.pop();

	    // Your code comes here.

	 } while (!std::empty(pipelines.front().commands));
         pipelines.pop();
      } while (std::empty(pipelines));
   }
}
```

The example above will start writing the `hello` command and proceed
reading its response. After that users can add further commands to the
queue. See the example directory for a complete example. The main
function looks like this

```cpp
int main()
{
   net::io_context ioc;
   net::ip::tcp::resolver resolver{ioc};
   auto const res = resolver.resolve("127.0.0.1", "6379");
   co_spawn(ioc, reader(res), net::detached);
   ioc.run();
}
```

See the `examples` directory for more examples.

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
