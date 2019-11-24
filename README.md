# Aedis

Aedis is a redis client designed with the following in mind

* Seamless integration with async code
* Based on Boost.Asio and the Networking TS
* Speed as a result of simplicity
* No overhead abstractions
* Easy and intuitive as clients for other languages

This library is header only. You only have to include `aedis.hpp` in your
project. Current dendencies are

* Boost.Asio
* libfmt

As of C++23 this library will have no external dependencies (assuming the
Networking TS gets finally merged in to the standard).

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
void example1()
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
   , {3, {"foobar"}}};

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

The following example shows how to specify the configuration options

```cpp
void example2()
{
   net::io_context ioc;

   session::config cfg
   { "127.0.0.1" // host
   , "6379" // port
   , 256 // Max pipeline size
   , std::chrono::milliseconds {500} // Connection retry
   , {} // Sentinel addresses
   , log::level::info
   };

   session s {ioc, cfg, "id"};

   s.send(ping());

   s.run();
   ioc.run();
}
```
The maximum pipeline size above refers to the blocks of commands sent
via the `session::send` function and not to the individual commands.
Logging is made using `std::clog`. The string `"id"` passed as third
argument to the session is prefixed to each log message.
Support for redis sentinel is still not implemented.

# Callbacks

It is possible to set some callbacks as shown in the example bellow

```cpp
void example3()
{
   net::io_context ioc;
   session s {ioc};

   s.set_on_conn_handler([]() {
      std::cout << "Connected" << std::endl;
   });

   s.set_msg_handler([](auto ec, auto res) {
      if (ec) {
         std::cerr << "Error: " << ec.message() << std::endl;
         return;
      }

      std::copy( std::cbegin(res)
               , std::cend(res)
               , std::ostream_iterator<std::string>(std::cout, " "));

      std::cout << std::endl;
   });

   s.send(ping());

   s.run();
   ioc.run();
}
```

# Missing features

The main missing features at the moment are

* Sentinel
* Cluster

I will implement those on demand.

