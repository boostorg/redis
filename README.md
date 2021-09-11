# Aedis

Aedis is a redis client designed for scalability and performance while
providing an easy and intuitive interface.

## Example

The general form of the read and write operations of a redis client
that support push notifications and pipelines looks like the following

```cpp
net::awaitable<void>
example(net::ip::tcp::socket& socket, std::queue<pipeline>& pipelines)
{
   pipelines.push({});
   pipelines.back().hello("3");

   std::string buffer;
   response_buffers buffers;
   response_adapters adapters{buffers};
   consumer_state cs;

   for (;;) {
      auto const type =
        co_await async_consume(
            socket, buffer, pipelines, adapters, cs, net::use_awaitable);

      if (type == resp3::type::push) {
         // Push received.
         continue;
      }

      auto const cmd = pipelines.front().commands.front();

      // Response to a specific command.
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

   net::ip::tcp::socket socket{ioc};
   net::connect(socket, res);

   std::queue<pipeline> pipelines;
   co_spawn(ioc, example(socket, pipelines), net::detached);
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
