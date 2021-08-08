# Aedis

Aedis is a redis client designed for scalability and performance while
providing an easy and intuitive interface. Some of the supported
features are

* RESP3 and STL containers.
* Pipelines (essential for performance).

## Tutorial

All you have to do is to define a receiver class as shown in the example
below

```cpp
   void f(request& req)
   {
      req.ping();
      req.quit();
   }

   class receiver : public receiver_base {
   private:
      std::shared_ptr<connection> conn_;

   public:
      receiver(std::shared_ptr<connection> conn) : conn_{conn} { }

      void on_hello(array_type& v) noexcept override
	 { conn_->send(f); }

      void on_ping(simple_string_type& s) noexcept override
	 { std::cout << "PING: " << s << std::endl; }

      void on_quit(simple_string_type& s) noexcept override
	 { std::cout << "QUIT: " << s << std::endl; }
   };
```

In general for each redis command you have to override a member
function in the receiver. The main function looks like this

```cpp
   int main()
   {
      net::io_context ioc {1};
      auto conn = std::make_shared<connection>(ioc);
      receiver recv{conn};
      conn->start(recv);
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
