# Aedis
Aedis is a redis client designed with the following in mind

* Simplicity and Minimalist
* No overhead abstractions
* Optimal use of Asio
* Async

# Example

Talking to a redis server is as simple as

```
void foo()
{
   net::io_context ioc;
   session ss {ioc};

   ss.send(ping());

   ss.run();
   ioc.run();
}
```

Composition of commands is trivial and there is support for some stl
containers

```
void foo()
{
   std::list<std::string> b
   {"one" ,"two", "three"};

   std::set<std::string> c
   {"a" ,"b", "c"};

   std::map<std::string, std::string> d
   { {{"Name"},      {"Marcelo"}} 
   , {{"Education"}, {"Physics"}}
   , {{"Job"},       {"Programmer"}}};

   std::map<int, std::string> e
   { {1, {"foo"}} 
   , {2, {"bar"}}
   , {3, {"foobar"}}
   };

   auto s = ping()
          + rpush("b", b)
          + lrange("b")
          + del("b")
          + multi()
          + rpush("c", c)
          + lrange("c")
          + del("c")
          + hset("d", d)
          + hvals("d")
          + zadd({"e"}, e)
          + zrange("e")
          + zrangebyscore("foo", 2, -1)
          + set("f", {"39"})
          + incr("f")
          + get("f")
          + expire("f", 10)
          + publish("g", "A message")
          + exec();

   net::io_context ioc;
   session ss {ioc};

   ss.send(std::move(s));

   ss.run();
   ioc.run();
}
```

NOTE: Not all commands are implemented yet. Since this client was
writen for my own use I implement new functionality on demand.

# Features

* Pubsub
* Pipeline
* Reconnection on connection lost.

The main missing features at the moment are

* Sentinel
* Cluster

I will implement those on demand.

# Intallation

Aedis is header only. You only have to include `aedis.hpp` in your
project. Further dependencies are 

* Boost.Asio
* libfmt

