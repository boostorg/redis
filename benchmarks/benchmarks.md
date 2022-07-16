# TCP echo server performance.

This document describes how I benchmarked a TCP echo server
implemented in different languages and frameworks.

## Motivation

The main motivations for choosing a TCP echo server as a benchmark
program are

   * Simple to implement in most languages.
   * Does not require expertise level get decent performance.
   * I/O bound: Echo servers have very low CPU consumption since they
     don't compute anything. It is therefore an excelent measure of
     the ability of a program to server concurrent requests.
   * It simulates very well a typical backend in regard to concurrency.

I also imposed some constraints on the implementations

   * It should not require me to write too much code, in other words,
     it must be simple.
   * Favor the use standard idioms.
   * Avoid optimizations that requires expert level or makes the
     code too complex e.g. connection and thread pool.

## Without Redis

First I tested a pure TCP echo server, i.e. that sends the messages
directly to the client without interacting with Redis. This is the
result

### Remarks:

   * I was not expecting Asio to perform so much better than the alternatives like Tokio and libuv.
   * I did expect nodejs to come a little behind given it is is
     javascript code. Otherwise I did expect it to have similar
     performance to libuv since it is the framework behind it.
   * The go performance was no surprise: decent and not some much far behind nodejs.

The tests were performed on the localhost where latency is 0.07ms on
average. On higher latency networks the difference among libraries
will decrease.

The code used in the benchmarks can be found at

   * [Asio](https://github.com/mzimbres/aedis/blob/3fb018ccc6138d310ac8b73540391cdd8f2fdad6/benchmarks/cpp/asio/echo_server_direct.cpp): A variation of [this](https://github.com/chriskohlhoff/asio/blob/4915cfd8a1653c157a1480162ae5601318553eb8/asio/src/examples/cpp20/coroutines/echo_server.cpp) Asio example.
   * [Libuv](): A variation of [this](https://github.com/libuv/libuv/blob/06948c6ee502862524f233af4e2c3e4ca876f5f6/docs/code/tcp-echo-server/main.c) Libuv example .
   * [Tokio](https://github.com/mzimbres/aedis/tree/3fb018ccc6138d310ac8b73540391cdd8f2fdad6/benchmarks/rust/echo_server_direct): This was taken from [here](https://docs.rs/tokio/latest/tokio/).
   * [Nodejs](https://github.com/mzimbres/aedis/tree/3fb018ccc6138d310ac8b73540391cdd8f2fdad6/benchmarks/nodejs/echo_server_direct).
   * [Go](https://github.com/mzimbres/aedis/blob/3fb018ccc6138d310ac8b73540391cdd8f2fdad6/benchmarks/go/echo_server_direct.go).

## With Redis

This is similar to the echo server described above but the message is
echoed by Redis and the Echo server works as a proxy between the
client and the Redis server. The result can be seen below

### Remarks

As the reader can see, the Libuv and the Rust test are not depicted
above, reasons are

   * [redis-rs](https://github.com/redis-rs/redis-rs): This client
     comes so far behind that it can't even be represented together
     with the other benchmarks without making them insignificant.
     I don't know for sure why it is so slow, I suppose however it is
     not implementing pipelines properly.

   * Libuv: I let this out because it would require too much work to
     make it have a good performance. More specifically, I would have
     to use hiredis and implement support for pipelines manually.


The code used in the benchmarks can be found at

   * [Aedis](https://github.com/mzimbres/aedis/blob/3fb018ccc6138d310ac8b73540391cdd8f2fdad6/examples/echo_server.cpp).
   * [Nodejs](https://github.com/mzimbres/aedis/tree/3fb018ccc6138d310ac8b73540391cdd8f2fdad6/benchmarks/nodejs/echo_server_over_redis).
   * [Go](https://github.com/mzimbres/aedis/blob/3fb018ccc6138d310ac8b73540391cdd8f2fdad6/benchmarks/go/echo_server_over_redis.go).

## Conclusion:

The main conclusion is that pipelines is fundamental, much more fundamental than the language performance per se.

If your spot any performance improvement in any of the example, please open a PR and I will gladly merge it.
