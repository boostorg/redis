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
net::awaitable<void> ping2()
{
   auto socket = co_await make_connection();
   resp3::stream<tcp_socket> stream{std::move(socket)};

   resp3::request req;
   req.push(command::hello, 3);
   req.push(command::ping);
   req.push(command::quit);
   co_await stream.async_write(req);

   while (!std::empty(req.commands)) {
      resp3::response resp;
      co_await stream.async_read(resp);
      req.commands.pop();
      std::cout << resp << std::endl;
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
