# Aedis

Aedis is a redis client designed for seamless integration with async code while
providing a easy and intuitive interface.  To use this library include
`aedis.hpp` in your project.

## Examples

The examples below will use coroutines, callbacks and futures are
supported as well.

### Basics

This is the simplest example possible. 

```cpp
awaitable<void> example1()
{
   auto ex = co_await this_coro::executor;

   tcp::resolver resv(ex);
   auto const r = resv.resolve("127.0.0.1", "6379");

   tcp_socket socket {ex};
   co_await async_connect(socket, r);

   std::list<std::string> list
   {"one" ,"two", "three"};

   std::map<std::string, std::string> map
   { {{"Name"},      {"Marcelo"}} 
   , {{"Education"}, {"Physics"}}
   , {{"Job"},       {"Programmer"}}
   };

   resp::pipeline p;
   p.rpush("list", list);
   p.lrange("list");
   p.del("list");
   p.hset("map", map);
   p.hincrby("map", "Age", 40);
   p.hmget("map", {"Name", "Education", "Job"});
   p.hvals("map");
   p.hlen("map");
   p.hgetall("map");
   p.quit();

   co_await async_write(socket, buffer(p.payload));

   resp::buffer buffer;
   for (;;) {
      resp::response res;
      co_await resp::async_read(socket, buffer, res);
      resp::print(res.res);
   }
}
```

Though short the example above ilustrates many important points

* STL containers are suported when appropriate.
* Commands are sent to redis in pipeline to improve performance.

