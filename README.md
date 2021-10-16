# Aedis

Aedis is a redis client designed for scalability and performance while
providing an easy and intuitive interface.

## Example

Lets us see some examples with increasing order of complexity.

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

The example above will start writing the `hello` command and proceed
reading its response. After that users can add further commands to the
queue. See the example directory for a complete example.

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
