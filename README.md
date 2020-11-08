# Aedis

Aedis is a redis client designed for

* Seamless integration with async code
* Easy and intuitive usage

To use this library include `aedis.hpp` in your project. Current dendencies are
`Boost.Asio` and `libfmt`. As of C++23 this library will have no external dependencies.

# Example

The examples below will use coroutines, callbacks and futures are
supported as well.

```cpp
awaitable<void> example1(tcp::resolver::results_type const& r)
{
   tcp_socket socket {co_await this_coro::executor};

   co_await async_connect(socket, r);

   auto cmd = ping();
   co_await async_write(socket, buffer(cmd));

   resp::buffer buffer;
   resp::response res;
   co_await resp::async_read(socket, buffer, res);

   resp::print(res.res);
}
```

Command pipelines can be generated easily

```cpp
awaitable<void> example2(tcp::resolver::results_type const& r)
{
   tcp_socket socket {co_await this_coro::executor};

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

STL containers are also suported

```cpp
awaitable<void> example3(tcp::resolver::results_type const& r)
{
   tcp_socket socket {co_await this_coro::executor};

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

   auto cmd = ping()
            + role()
            + flushall()
            + rpush("a", a)
            + lrange("a")
            + del("a")
            + multi()
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
            + zrangebyscore("foo", 2, -1)
            + set("f", {"39"})
            + incr("f")
            + get("f")
            + expire("f", 10)
            + publish("g", "A message")
            + exec()
	    + set("h", {"h"})
	    + append("h", "h")
	    + get("h")
	    + auth("password")
	    + bitcount("h")
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

