# TCP echo server performance.

The main motivations for choosing an echo server to benchmark a redis
client were

   * Simple to implement in most languages.
   * Does not require expertise level get decent performance.
   * Excelent to measure ability to server concurrent requests.
   * It must test the ability to deal with concurrency.
   * I/O bound: In echo servers CPU consumption is very low as a result
     the performance is more related to how well concurrency is
     implemented.

I also imposed some constraints on the implementations

   * Favor simple implementations that use standard idioms of their
     specific language or framework.
   * Avoid of optimization that makes it too complex e.g. connection
     pool or that requires expert level.

## Without Redis

Before we can compare the Redis clients implementation we must have a
rough view about how different libraries compare in terms of
performance.

Remarks:

   * I was not expecting Asio to perform so much better than the alternatives that Tokio and libuv.
   * I did expect nodejs to come a little behind given it is
     implemented in javascript. Otherwise I expected it to have
     similar performance to libuv since it is the framework behind
     nodejs.

The code can be found at

   * Asio: A variation of the asio example.
   * Libuv: Copy and pasted of the libuv example.
   * Tokio: Copy and pasted of the example provided here.
   * Go: Found in the internet and make adjustmenst.

## With Redis

In this case the message is not echoed directly to the client but sent
to the server with the Ping command which will echo it back to the
server, only after that it is sent to the client.

the set is.

The most remarkable thing here is that the Rust Redis client comes so
far behind that it can't even be represented together with the other
benchmarks.

The Reason why the Aedis and the nodejs client are so much faster than
the alternatives is that they implement pipeline.

The code I used is a variation of the list given above, that I
implemented myself. The Redis client for each language were chosen
based on their number of stars as given here.

## Conclusion:

The main conclusion is that pipelines is fundamental, much more
fundamental than the language performance per se.

If your spot any performance improvement in any of the example, please
open a PR and I will gladly merge it.
