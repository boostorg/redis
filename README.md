# Aedis
Aedis is a redis client designed with the following in mind

* Simplicity and Minimalism
* No overhead abstractions
* Optimal use of Boost.Asio
* Async

# Example

Talking to a redis server is as simple as

```cpp
void send(std::string cmd)
{
   net::io_context ioc;
   session s {ioc};

   s.send(cmd);

   s.run();
   ioc.run();
}
```

Composition of commands is trivial and there is support for some stl
containers

```cpp
void foo()
{
   std::list<std::string> a
   {"one" ,"two", "three"};

   std::set<std::string> b
   {"a" ,"b", "c"};

   std::map<std::string, std::string> c
   { {{"Name"},      {"Marcelo"}} 
   , {{"Education"}, {"Physics"}}
   , {{"Job"},       {"Programmer"}}};

   std::map<int, std::string> d
   { {1, {"foo"}} 
   , {2, {"bar"}}
   , {3, {"foobar"}}
   };

   auto s = ping()
          + rpush("a", a)
          + lrange("a")
          + del("a")
          + multi()
          + rpush("b", b)
          + lrange("b")
          + del("b")
          + hset("c", c)
          + hvals("c")
          + zadd({"d"}, d)
          + zrange("d")
          + zrangebyscore("foo", 2, -1)
          + set("f", {"39"})
          + incr("f")
          + get("f")
          + expire("f", 10)
          + publish("g", "A message")
          + exec();

  send(std::move(s));
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

