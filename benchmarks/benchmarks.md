# TCP echo server performance.

This document describe benchmakrs I made in implementations of TCP
echo server I implemented in different languages.  The main
motivations for choosing a TCP echo server as a benchmark program are

   * Simple to implement and does not require expertise level in most languages.
   * I/O bound: Echo servers have very low CPU consumption in general
     and  therefore an excelent measure of the ability of a program to
     server concurrent requests.
   * It simulates very well a typical backend in regard to concurrency.

I also imposed some constraints on the implementations

   * It should not require me to write too much code.
   * Favor the use standard idioms and avoid optimizations that require expert level.
   * Makes no use of complex things like connection and thread pool.

## No Redis

First I tested a pure TCP echo server, i.e. one that sends the messages
directly to the client without interacting with Redis. The result can
be seen below

![](https://mzimbres.github.io/aedis/tcp-echo-direct.png)

The tests were performed with 1000 TCP connection on the localhost
where latency is 0.07ms on average. On higher latency networks the
difference among libraries is expected to decrease. 

### Remarks:

   * I was not expecting Asio to perform so much better than Tokio and libuv.
   * I did expect nodejs to come a little behind given it is is
     javascript code. Otherwise I did expect it to have similar
     performance to libuv since it is the framework behind it.
   * The go performance was no surprise: decent and not some much far behind nodejs.

The code used in the benchmarks can be found at

   * [Asio](https://github.com/mzimbres/aedis/blob/3fb018ccc6138d310ac8b73540391cdd8f2fdad6/benchmarks/cpp/asio/echo_server_direct.cpp): A variation of [this](https://github.com/chriskohlhoff/asio/blob/4915cfd8a1653c157a1480162ae5601318553eb8/asio/src/examples/cpp20/coroutines/echo_server.cpp) Asio example.
   * [Libuv](https://github.com/mzimbres/aedis/tree/835a1decf477b09317f391eddd0727213cdbe12b/benchmarks/c/libuv): Taken from [here](https://github.com/libuv/libuv/blob/06948c6ee502862524f233af4e2c3e4ca876f5f6/docs/code/tcp-echo-server/main.c) Libuv example .
   * [Tokio](https://github.com/mzimbres/aedis/tree/3fb018ccc6138d310ac8b73540391cdd8f2fdad6/benchmarks/rust/echo_server_direct): Taken from [here](https://docs.rs/tokio/latest/tokio/).
   * [Nodejs](https://github.com/mzimbres/aedis/tree/3fb018ccc6138d310ac8b73540391cdd8f2fdad6/benchmarks/nodejs/echo_server_direct)
   * [Go](https://github.com/mzimbres/aedis/blob/3fb018ccc6138d310ac8b73540391cdd8f2fdad6/benchmarks/go/echo_server_direct.go)

## Echo over Redis

This is similar to the echo server described above but the message is
echoed by Redis. The echo server works as a proxy between the
client and the Redis server. The result can be seen below

![](https://mzimbres.github.io/aedis/tcp-echo-over-redis.png)

The tests were also performed with 1000 TCP connections on a network
latency is 35ms on average.

### Remarks

As the reader can see, the Libuv and the Rust test are not depicted
above, reasons are

   * [redis-rs](https://github.com/redis-rs/redis-rs): This client
     comes so far behind that it can't even be represented together
     with the other benchmarks without making them insignificant.  I
     don't know for sure why it is so slow, I suppose however it has
     something to do with its lack of proper
     [pipelining](https://redis.io/docs/manual/pipelining/) support.
     In fact, the more TCP connections I lauch the worst its
     performance gets.

   * Libuv: I left it out because it would require too much work to
     make it have a good performance. More specifically, I would have
     to use hiredis and implement support for pipelines manually.

The code used in the benchmarks can be found at

   * [Aedis](https://github.com/mzimbres/aedis): [Code](https://github.com/mzimbres/aedis/blob/3fb018ccc6138d310ac8b73540391cdd8f2fdad6/examples/echo_server.cpp)
   * [node-redis](https://github.com/redis/node-redis): [Code](https://github.com/mzimbres/aedis/tree/3fb018ccc6138d310ac8b73540391cdd8f2fdad6/benchmarks/nodejs/echo_server_over_redis)
   * [go-redis](https://github.com/go-redis/redis): [Go](https://github.com/mzimbres/aedis/blob/3fb018ccc6138d310ac8b73540391cdd8f2fdad6/benchmarks/go/echo_server_over_redis.go)

## Contributing

If your spot any performance improvement in any of the example or
would like to include other clients, please open a PR and I will
gladly merge it.
