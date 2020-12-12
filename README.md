# Aedis

Aedis is a redis client designed for seamless integration with async code while
providing a easy and intuitive interface.  To use this library include
`aedis.hpp` in your project.

## Tutoria and examples

Below we show how to use the library focused in sync and async code.

### Sync

```cpp
void sync_example1()
{
   io_context ioc {1};

   tcp::resolver resv(ioc);
   tcp::socket socket {ioc};
   net::connect(socket, resv.resolve("127.0.0.1", "6379"));

   resp::pipeline p;
   p.ping();

   net::write(socket, buffer(p.payload));

   resp::buffer buffer;
   resp::response res;
   resp::read(socket, buffer, res);

   // res.result contains the response as std::vector<std::string>.
}
```

The example above is overly simple. In real world cases it is
necessary, for many reasons to keep reading from the socket, for
example to detect the connection has been lost or to be able to deal
with redis unsolicited events. A more realistic example therefore is

```cpp
void sync_example2()
{
   io_context ioc {1};

   tcp::resolver resv(ioc);
   tcp::socket socket {ioc};
   net::connect(socket, resv.resolve("127.0.0.1", "6379"));

   resp::pipeline p;
   p.multi();
   p.ping();
   p.set("Name", {"Marcelo"});
   p.incr("Age");
   p.exec();
   p.quit();

   net::write(socket, buffer(p.payload));

   resp::buffer buffer;
   for (;;) {
      boost::system::error_code ec;
      resp::response res;
      resp::read(socket, buffer, res, ec);
      if (ec) {
	 std::cerr << ec.message() << std::endl;
	 break;
      }
      resp::print(res.result);
   }
}
```

In this example we add more commands to the pipeline, they will be all
sent together to redis improving performance. Second we keep reading
until the socket is closed by redis after it receives the quit
command.

### Async

The sync examples above are good as introduction and can be also used
in production. However to don't scale, this is specially problematic
on backends. Fourtunately in C++20 it became trivial to convert the 
sync into asyn code. The example below shows an example.

```cpp
net::awaitable<void> async_example1()
{
   auto ex = co_await this_coro::executor;

   tcp::resolver resv(ex);
   auto const r = resv.resolve("127.0.0.1", "6379");

   tcp_socket socket {ex};
   co_await async_connect(socket, r);

   std::map<std::string, std::string> map
   { {{"Name"},      {"Marcelo"}} 
   , {{"Education"}, {"Physics"}}
   , {{"Job"},       {"Programmer"}}
   };

   resp::pipeline p;
   p.hset("map", map);
   p.hincrby("map", "Age", 40);
   p.hmget("map", {"Name", "Education", "Job"});
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

