# Aedis

Aedis is a redis client designed for scalability and performance while
providing an easy and intuitive interface. Some of the supported
features are

* TLS, RESP3 and STL containers.
* Pipelines (essential for performance).
* Coroutines, futures and callbacks.

## Tutorial

All you have to do is to define an `enum` that defines the events you
want to handle, if any, and a receiver. For example

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

      void on_hello(resp::array_type& v) noexcept override
	 { conn_->send(f); }

      void on_ping(resp::simple_string_type& s) noexcept override
	 { std::cout << "PING: " << s << std::endl; }

      void on_quit(resp::simple_string_type& s) noexcept override
	 { std::cout << "QUIT: " << s << std::endl; }
   };

```

Inheriting from the `receiver_base` class is not needed but convenient
to avoid writing the complete receiver interface. In general for each
redis command you have to override (or offer) a member function in the
receiver.

The main function looks like

```cpp
   int main()
   {
      net::io_context ioc {1};
      net::ip::tcp::resolver resolver{ioc};
      auto const results = resolver.resolve("127.0.0.1", "6379");
      auto conn = std::make_shared<connection>(ioc);
      receiver recv{conn};
      conn->start(recv, results);
      ioc.run();
   }
```

