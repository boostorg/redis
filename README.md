# Aedis

Aedis is a redis client designed for

* Seamless integration with async code.
* Easy and intuitive usage.

To use this library include `aedis.hpp` in your project. Current dendencies are
`Boost.Asio` and `libfmt`. As of C++23 this library will have no external dependencies.

# Examples

The examples below will use coroutines, callbacks and futures are
supported as well.

## Ping

This is the simplest example possible. 

1. Connect to the redis server in the localhost
1. Send a ping command
1. Parse the output use it and leave.

```cpp
awaitable<void> example1()
{
   auto ex = co_await this_coro::executor;

   tcp::resolver resv(ex);
   auto const r = resv.resolve("127.0.0.1", "6379");

   tcp_socket socket {ex};
   co_await async_connect(socket, r);

   auto cmd = ping() + quit();
   co_await async_write(socket, buffer(cmd));

   resp::buffer buffer;
   resp::response res;
   co_await resp::async_read(socket, buffer, res);

   resp::print(res.res);
}
```

## Pipeline

Same as above but with a more complex command. We have to keep reading
from the socket untill all commands responses have been parsed.

```cpp
awaitable<void> example2()
{
   auto ex = co_await this_coro::executor;

   tcp::resolver resv(ex);
   auto const r = resv.resolve("127.0.0.1", "6379");

   tcp_socket socket {ex};

   co_await async_connect(socket, r);

   auto cmd = multi()
            + ping()
            + incr("age")
            + exec()
	    + quit()
	    ;

   co_await async_write(socket, buffer(cmd));

   resp::buffer buffer;
   for (;;) {
      resp::response res;
      co_await resp::async_read(socket, buffer, res);
      resp::print(res.res);
   }
}
```

## STL containers

Some commands and data structures in redis can be mapped in STL
containers. The example below shows some of them

```cpp
awaitable<void> example3()
{
   auto ex = co_await this_coro::executor;

   tcp::resolver resv(ex);
   auto const r = resv.resolve("127.0.0.1", "6379");

   tcp_socket socket {ex};
   co_await async_connect(socket, r);

   std::list<std::string> a
   {"one" ,"two", "three"};

   std::set<std::string> b
   {"a" ,"b", "c"};

   std::map<std::string, std::string> c
   { {{"Name"},      {"Marcelo"}} 
   , {{"Education"}, {"Physics"}}
   , {{"Job"},       {"Programmer"}}
   };

   std::map<int, std::string> d
   { {1, {"foo"}} 
   , {2, {"bar"}}
   , {3, {"foobar"}}
   };

   auto cmd = rpush("a", a)
            + lrange("a")
            + del("a")
            + rpush("b", b)
            + lrange("b")
            + del("b")
            + hset("c", c)
            + hincrby("c", "Age", 40)
            + hmget("c", {"Name", "Education", "Job"})
            + hvals("c")
            + hlen("c")
            + hgetall("c")
            + zadd({"d"}, d)
            + zrange("d")
	    + quit()
	    ;

   co_await async_write(socket, buffer(cmd));

   resp::buffer buffer;
   for (;;) {
      resp::response res;
      co_await resp::async_read(socket, buffer, res);
      resp::print(res.res);
   }
}
```

