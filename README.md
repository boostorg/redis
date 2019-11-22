# Aedis

Aedis is a redis client designed with the following in mind

* Seamless integration with async code
* Based on Boost.Asio
* Speed as a result of simplicity
* No overhead abstractions
* Easy and intuitive as clients for other languages

# Example

Sending a command to a redis server is as simple as

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

Commands can be generated easily and there is support for STL
containers when it makes sense

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

# Features

* Pubsub
* Pipeline
* Reconnection on connection lost.

The main missing features at the moment are

* Sentinel
* Cluster

I will implement those on demand.

# Installation

Aedis is header only. You only have to include `aedis.hpp` in your
project. Further dependencies are 

* Boost.Asio
* libfmt

