
# Documentation

[TOC]

## Overview

Aedis is a high-level [Redis](https://redis.io/) client library
built on top of
[Asio](https://www.boost.org/doc/libs/release/doc/html/boost_asio.html).
Some of its distinctive features are

* Support for the latest version of the Redis communication protocol [RESP3](https://github.com/redis/redis-specifications/blob/master/protocol/RESP3.md).
* Support for STL containers, TLS and Redis sentinel.
* Serialization and deserialization of your own data types.
* Healthy checks, back pressure and low latency.

In addition to that, Aedis hides most of the low-level Asio code away
from the user, which in the majority of the use cases will interact
with only three library entities

* `aedis::connection`: A healthy long-lasting connection to the Redis server.
* `aedis::resp3::request`: A container of Redis commands.
* `aedis::adapt()`: Adapts user data structures like STL containers to
  receive Redis responses.

Let us see how this works in more detail.

### Connection

The code below will establish a connection with a Redis
server where users can send commands (see intro.cpp)

```cpp
int main()
{
   net::io_context ioc;
   connection conn{ioc};

   conn.async_run({"127.0.0.1", "6379"}, {}, [](auto ec) { ... });

   // Pass conn to other operations ...

   ioc.run();
}
```

Requests on the other hand can be sent at any time, regardless of whether before or
after a connection was established. For example, the code below sends
the `PING` and `QUIT` command, waits for the response and exits

```cpp
net::awaitable<void> ping(std::shared_ptr<connection> conn)
{
   // Request
   request req;
   req.push("PING", "some message");
   req.push("QUIT");

   // Response
   std::tuple<std::string, aedis::ignore> resp;

   // Execution
   co_await conn->async_exec(req, adapt(resp));
   std::cout << "Response: " << std::get<0>(resp) << std::endl;
}
```

The structure of how to send commands is evident from the code above

* Create a `aedis::resp3::request` object and add commands.
* Declare responses as elements of a `std::tuple`.
* Execute the request.

Multiple calls to `connection::async_exec` are synchronized
automatically so that different operations (or coroutines) don't have
to be aware of each other. Server side pushes can be received on the
same connection object that is being used to execute commands, for
example (see subscriber.cpp)

```cpp
net::awaitable<void> receive_pushes(connection& db)
{
   for (std::vector<node<std::string>> resp;;) {
      co_await db->async_receive_push(adapt(resp));
      // Process the push in resp.
      resp.clear();
   }
}
```

@note Users should make sure any server pushes sent by the server are
consumed, otherwise the connection will eventually timeout.

### Reconnect

The `aedis::connection` class also supports reconnection. In simple
scenarios for example, where reconnecting to the same server is enough
a loop like shown below is enough to provide this functionality

```cpp
net::awaitable<void> reconnect(std::shared_ptr<connection> db)
{
   net::steady_timer timer{co_await net::this_coro::executor};
   endpoint ep{"127.0.0.1", "6379"};
   for (;;) {
      boost::system::error_code ec;
      co_await db->async_run(ep, req, adapt(), {}, net::redirect_error(net::use_awaitable, ec));
      db->reset_stream();
      timer.expires_after(std::chrono::seconds{1});
      co_await timer.async_wait();
   }
}
```
more complex scenarios, like performing a failover with sentinel can
be found in the examples. Calls to `connection::async_exec` won't
automatically fail as a result of connection lost, rather, they will
remain suspended until a new connection is established, after that
all requests are sent automatically. This behaviour can be
changed per request by setting on the
`aedis::resp3::request::config::close_on_connection_lost` or by calling
`connection::cancel()` with `connection::operation::exec`
which will cause all pending requests to be canceled.

### Timeouts

Aedis high-level API provides built-in support for most timeouts users
might need. For example, the `aedis::connection::async_run` member
function performs the following operations on behalf of the user

* Resolves Redis address.
* Connects to the endpoint.
* TLS handhshake (for TLS endpoints).
* RESP3 handshake, authentication and role check.
* Keeps sending PING commands to check for unresponsive servers.
* Keeps reading from the socket to handle server pushes and command responses.
* Keeps writing requests as it becomes possible e.g. after last response has arrived.

To control the timeout-behaviour of the operations above users must
create a `aedis::connection::timeouts` and pass it to as argument to
the `aedis::connection::async_run` member function (or use the
suggested defaults).

Another important topic regarding timeouts is the cancellation of
`aedis::connection::async_exec`.  With the introduction of awaitable
operators in Asio it is very simple implement timeouts either on
individual or on a group of operations.  Users, for example, may be
tempted in writing code like

```cpp
co_await (conn.async_exec(...) || timer.async_wait(...))
```

the problem with this approach in Aedis is that to improve performance
Redis encourages the use of pipelines, where many requests are sent in
a single chunk to the server. In this scenario it is harder to cancel
individual operations without causing all other (independent) requests
in the same pipeline to fail too.

### Installation

Download the latest Aedis release from github 

```cpp
$ wget https://github.com/mzimbres/aedis/releases/download/v1.1.0/aedis-1.1.0.tar.gz
```

and unpack in your preferred location. Aedis is a header only
library, so you can starting using it. For that include the
following header 

```cpp
#include <aedis/src.hpp>

```
in no more than one source file in your applications (see intro.cpp
for example). To build the examples and run the tests cmake is also
supported

```cpp
$ BOOST_ROOT=/opt/boost_1_79_0/ cmake
$ make
$ make test
```

These are the requirements for using Aedis

- Boost 1.79 or greater.
- C++17. Some examples require C++20 with coroutine support.
- Redis 6 or higher. Optionally also redis-cli and Redis Sentinel.

The following compilers are supported

- Tested with gcc: 10, 11, 12.
- Tested with clang: 11, 13, 14.


### Examples

Users are encouraged to skim over the examples below before proceeding
to the next sections

* intro.cpp: The Aedis hello-world program. It sends one command to Redis and quits the connection.
* intro_tls.cpp: Same as intro.cpp but over TLS.
* intro_sync.cpp: Synchronous version of intro.cpp.
* containers.cpp: Shows how to send and receive stl containers and how to use transactions.
* serialization.cpp: Shows how to serialize types using Boost.Json.
* subscriber.cpp: Shows how to implement pubsub that reconnects and resubscribes when the connection is lost.
* subscriber_sentinel.cpp: Same as subscriber.cpp but with failover with sentinels.
* echo_server.cpp: A simple TCP echo server.
* chat_room.cpp: A simple chat room.

<a name="requests"></a>

### API Reference

* [High-Level](#high-level-api): Recommend to all users
* [Low-Level](#low-level-api): For users with needs yet to be imagined by the author.

In the next sections we will see how to create requests and receive
responses with more detail

## Requests

Redis requests are composed of one of more Redis commands (in
Redis documentation they are called
[pipelines](https://redis.io/topics/pipelining)). For example

```cpp
request req;

// Command with variable length of arguments.
req.push("SET", "key", "some value", value, "EX", "2");

// Pushes a list.
std::list<std::string> list
   {"channel1", "channel2", "channel3"};
req.push_range("SUBSCRIBE", list);

// Same as above but as an iterator range.
req.push_range2("SUBSCRIBE", std::cbegin(list), std::cend(list));

// Pushes a map.
std::map<std::string, mystruct> map
   { {"key1", "value1"}
   , {"key2", "value2"}
   , {"key3", "value3"}};
req.push_range("HSET", "key", map);
```

Sending a request to Redis is then performed with the following function

```cpp
co_await db->async_exec(req, adapt(resp));
```

<a name="serialization"></a>

### Serialization

The `push` and `push_range` functions above work with integers
e.g. `int` and `std::string` out of the box. To send your own
data type defined a `to_bulk` function like this

```cpp
struct mystruct {
   // Example struct.
};

void to_bulk(std::string& to, mystruct const& obj)
{
   std::string dummy = "Dummy serializaiton string.";
   aedis::resp3::to_bulk(to, dummy);
}
```

Once `to_bulk` is defined and accessible over ADL `mystruct` can
be passed to the `request`

```cpp
request req;

std::map<std::string, mystruct> map {...};

req.push_range("HSET", "key", map);
```

Example serialization.cpp shows how store json string in Redis.

<a name="responses"></a>

## Responses

To read responses effectively, users must know their RESP3 type,
this can be found in the Redis documentation for each command
(https://redis.io/commands). For example

Command  | RESP3 type                          | Documentation
---------|-------------------------------------|--------------
lpush    | Number                              | https://redis.io/commands/lpush
lrange   | Array                               | https://redis.io/commands/lrange
set      | Simple-string, null or blob-string  | https://redis.io/commands/set
get      | Blob-string                         | https://redis.io/commands/get
smembers | Set                                 | https://redis.io/commands/smembers
hgetall  | Map                                 | https://redis.io/commands/hgetall

Once the RESP3 type of a given response is known we can choose a
proper C++ data structure to receive it in. Fortunately, this is a
simple task for most types. The table below summarises the options

RESP3 type     | Possible C++ type                                            | Type
---------------|--------------------------------------------------------------|------------------
Simple-string  | `std::string`                                              | Simple
Simple-error   | `std::string`                                              | Simple
Blob-string    | `std::string`, `std::vector`                               | Simple
Blob-error     | `std::string`, `std::vector`                               | Simple
Number         | `long long`, `int`, `std::size_t`, `std::string`           | Simple
Double         | `double`, `std::string`                                    | Simple
Null           | `std::optional<T>`                                         | Simple
Array          | `std::vector`, `std::list`, `std::array`, `std::deque`     | Aggregate
Map            | `std::vector`, `std::map`, `std::unordered_map`            | Aggregate
Set            | `std::vector`, `std::set`, `std::unordered_set`            | Aggregate
Push           | `std::vector`, `std::map`, `std::unordered_map`            | Aggregate

For example

```cpp
request req;
req.push("HELLO", 3);
req.push_range("RPUSH", "key1", vec);
req.push_range("HSET", "key2", map);
req.push("LRANGE", "key3", 0, -1);
req.push("HGETALL", "key4");
req.push("QUIT");

std::tuple<
   aedis::ignore,  // hello
   int,            // rpush
   int,            // hset
   std::vector<T>, // lrange
   std::map<U, V>, // hgetall
   std::string     // quit
> resp;

co_await db->async_exec(req, adapt(resp));
```

The tag @c aedis::ignore can be used to ignore individual
elements in the responses. If the intention is to ignore the
response to all commands in the request use @c adapt()

```cpp
co_await db->async_exec(req, adapt());
```

Responses that contain nested aggregates or heterogeneous data
types will be given special treatment later in [the-general-case](#the-general-case).  As
of this writing, not all RESP3 types are used by the Redis server,
which means in practice users will be concerned with a reduced
subset of the RESP3 specification.

### Null

It is not uncommon for apps to access keys that do not exist or
that have already expired in the Redis server, to deal with these
cases Aedis provides support for `std::optional`. To use it,
wrap your type around `std::optional` like this

```cpp
std::optional<std::unordered_map<T, U>> resp;
co_await db->async_exec(req, adapt(resp));
```

Everything else stays the same.

### Transactions

To read the response to transactions we have to observe that Redis
queues the commands as they arrive and sends the responses back to
the user as an array, in the response to the @c exec command.
For example, to read the response to the this request

```cpp
db.send("MULTI");
db.send("GET", "key1");
db.send("LRANGE", "key2", 0, -1);
db.send("HGETALL", "key3");
db.send("EXEC");
```

use the following response type

```cpp
using aedis::ignore;

using exec_resp_type = 
   std::tuple<
      std::optional<std::string>, // get
      std::optional<std::vector<std::string>>, // lrange
      std::optional<std::map<std::string, std::string>> // hgetall
   >;

std::tuple<
   ignore,     // multi
   ignore,     // get
   ignore,     // lrange
   ignore,     // hgetall
   exec_resp_type, // exec
> resp;

co_await db->async_exec(req, adapt(resp));
```

Note that above we are not ignoring the response to the commands
themselves but whether they have been successfully queued. For a
complete example see containers.cpp.

### Deserialization

As mentioned in \ref serialization, it is common to
serialize data before sending it to Redis e.g.  to json strings.
For performance and convenience reasons, we may also want to
deserialize it directly in its final data structure. Aedis
supports this use case by calling a user provided `from_bulk`
function while parsing the response. For example

```cpp
void from_bulk(mystruct& obj, char const* p, std::size_t size, boost::system::error_code& ec)
{
   // Deserializes p into obj.
}
```

After that, you can start receiving data efficiently in the desired
types e.g. `mystruct`, `std::map<std::string, mystruct>` etc.

<a name="the-general-case"></a>

### The general case

There are cases where responses to Redis
commands won't fit in the model presented above, some examples are

* Commands (like `set`) whose responses don't have a fixed
RESP3 type. Expecting an `int` and receiving a blob-string
will result in error.
* RESP3 aggregates that contain nested aggregates can't be read in STL containers.
* Transactions with a dynamic number of commands can't be read in a `std::tuple`.

To deal with these cases Aedis provides the `resp3::node`
type, that is the most general form of an element in a response,
be it a simple RESP3 type or an aggregate. It is defined like this

```cpp
template <class String>
struct node {
   // The RESP3 type of the data in this node.
   type data_type;

   // The number of elements of an aggregate (or 1 for simple data).
   std::size_t aggregate_size;

   // The depth of this node in the response tree.
   std::size_t depth;

   // The actual data. For aggregate types this is always empty.
   String value;
};
```

Any response to a Redis command can be received in a
`std::vector<node<std::string>>`.  The vector can be seen as a
pre-order view of the response tree.  Using it is no different than
using other types

```cpp
// Receives any RESP3 simple data type.
node<std::string> resp;
co_await db->async_exec(req, adapt(resp));

// Receives any RESP3 simple or aggregate data type.
std::vector<node<std::string>> resp;
co_await db->async_exec(req, adapt(resp));
```

For example, suppose we want to retrieve a hash data structure
from Redis with `HGETALL`, some of the options are

* `std::vector<node<std::string>`: Works always.
* `std::vector<std::string>`: Efficient and flat, all elements as string.
* `std::map<std::string, std::string>`: Efficient if you need the data as a `std::map`.
* `std::map<U, V>`: Efficient if you are storing serialized data. Avoids temporaries and requires `from_bulk` for `U` and `V`.

In addition to the above users can also use unordered versions of the containers. The same reasoning also applies to sets e.g. `SMEMBERS`.

## Why Aedis

At the time of this writing there are seventeen Redis clients
listed in the [official](https://redis.io/docs/clients/#cpp) list.
With so many clients available it is not unlikely that users are
asking themselves why yet another one.  In this section I will try
to compare Aedis with the most popular clients and why we need
Aedis. Notice however that this is ongoing work as comparing
client objectively is difficult and time consuming.

Before we start it is worth mentioning some of the things it does
not support

* RESP3. Without RESP3 is impossible to support some important Redis features like client side caching, among other things.
* Coroutines.
* Reading responses directly in user data structures avoiding temporaries.
* Error handling with error-code and exception overloads.
* Healthy checks.

The remaining points will be addressed individually.

### Redis-plus-plus

The most popular client at the moment of this writing ranked by
github stars is

* https://github.com/sewenew/redis-plus-plus

Let us first have a look at what sending a command a pipeline and a
transaction look like

```cpp
auto redis = Redis("tcp://127.0.0.1:6379");

// Send commands
redis.set("key", "val");
auto val = redis.get("key"); // val is of type OptionalString.
if (val)
    std::cout << *val << std::endl;

// Sending pipelines
auto pipe = redis.pipeline();
auto pipe_replies = pipe.set("key", "value")
                        .get("key")
                        .rename("key", "new-key")
                        .rpush("list", {"a", "b", "c"})
                        .lrange("list", 0, -1)
                        .exec();

// Parse reply with reply type and index.
auto set_cmd_result = pipe_replies.get<bool>(0);
// ...

// Sending a transaction
auto tx = redis.transaction();
auto tx_replies = tx.incr("num0")
                    .incr("num1")
                    .mget({"num0", "num1"})
                    .exec();

auto incr_result0 = tx_replies.get<long long>(0);
// ...
```

Some of the problems with this API are

* Heterogeneous treatment of commands, pipelines and transaction. This makes auto-pipelining impossible.
* Any Api that sends individual commands has a very restricted scope of usability and should be avoided for performance reasons.
* The API imposes exceptions on users, no error-code overload is provided.
* No way to reuse the buffer for new calls to e.g. redis.get in order to avoid further dynamic memory allocations.
* Error handling of resolve and connection not clear.

According to the documentation, pipelines in redis-plus-plus have
the following characteristics

> NOTE: By default, creating a Pipeline object is NOT cheap, since
> it creates a new connection.

This is clearly a downside of the API as pipelines should be the
default way of communicating and not an exception, paying such a
high price for each pipeline imposes a severe cost in performance.
Transactions also suffer from the very same problem.

> NOTE: Creating a Transaction object is NOT cheap, since it
> creates a new connection.

In Aedis there is no difference between sending one command, a
pipeline or a transaction because requests are decoupled
from the IO objects.

> redis-plus-plus also supports async interface, however, async
> support for Transaction and Subscriber is still on the way.
> 
> The async interface depends on third-party event library, and so
> far, only libuv is supported.

Async code in redis-plus-plus looks like the following

```cpp
auto async_redis = AsyncRedis(opts, pool_opts);

Future<string> ping_res = async_redis.ping();

cout << ping_res.get() << endl;
```
As the reader can see, the async interface is based on futures
which is also known to have a bad performance.  The biggest
problem however with this async design is that it makes it
impossible to write asynchronous programs correctly since it
starts an async operation on every command sent instead of
enqueueing a message and triggering a write when it can be sent.
It is also not clear how are pipelines realised with the design
(if at all).

### Benchmark: Echo server

This document benchmarks the performance of TCP echo servers I
implemented in different languages using different Redis clients.  The
main motivations for choosing an echo server are

   * Simple to implement and does not require expertise level in most languages.
   * I/O bound: Echo servers have very low CPU consumption in general
     and  therefore are excelent to  measure how a program handles concurrent requests.
   * It simulates very well a typical backend in regard to concurrency.

I also imposed some constraints on the implementations

   * It should be simple enough and not require writing too much code.
   * Favor the use standard idioms and avoid optimizations that require expert level.
   * Avoid the use of complex things like connection and thread pool.

To reproduce these results run one of the echo-server programs in one
terminal and the
[echo-server-client](https://github.com/mzimbres/aedis/blob/42880e788bec6020dd018194075a211ad9f339e8/benchmarks/cpp/asio/echo_server_client.cpp)
in another.

#### Without Redis

First I tested a pure TCP echo server, i.e. one that sends the messages
directly to the client without interacting with Redis. The result can
be seen below

![](https://mzimbres.github.io/aedis/tcp-echo-direct.png)

The tests were performed with a 1000 concurrent TCP connections on the
localhost where latency is 0.07ms on average on my machine. On higher
latency networks the difference among libraries is expected to
decrease. 

   * I expected Libuv to have similar performance to Asio and Tokio.
   * I did expect nodejs to come a little behind given it is is
     javascript code. Otherwise I did expect it to have similar
     performance to libuv since it is the framework behind it.
   * Go did surprise me: faster than nodejs and liuv!

The code used in the benchmarks can be found at

   * [Asio](https://github.com/mzimbres/aedis/blob/3fb018ccc6138d310ac8b73540391cdd8f2fdad6/benchmarks/cpp/asio/echo_server_direct.cpp): A variation of [this](https://github.com/chriskohlhoff/asio/blob/4915cfd8a1653c157a1480162ae5601318553eb8/asio/src/examples/cpp20/coroutines/echo_server.cpp) Asio example.
   * [Libuv](https://github.com/mzimbres/aedis/tree/835a1decf477b09317f391eddd0727213cdbe12b/benchmarks/c/libuv): Taken from [here](https://github.com/libuv/libuv/blob/06948c6ee502862524f233af4e2c3e4ca876f5f6/docs/code/tcp-echo-server/main.c) Libuv example .
   * [Tokio](https://github.com/mzimbres/aedis/tree/3fb018ccc6138d310ac8b73540391cdd8f2fdad6/benchmarks/rust/echo_server_direct): Taken from [here](https://docs.rs/tokio/latest/tokio/).
   * [Nodejs](https://github.com/mzimbres/aedis/tree/3fb018ccc6138d310ac8b73540391cdd8f2fdad6/benchmarks/nodejs/echo_server_direct)
   * [Go](https://github.com/mzimbres/aedis/blob/3fb018ccc6138d310ac8b73540391cdd8f2fdad6/benchmarks/go/echo_server_direct.go)

#### With Redis

This is similar to the echo server described above but messages are
echoed by Redis and not by the echo-server itself, which acts
as a proxy between the client and the Redis server. The results
can be seen below

![](https://mzimbres.github.io/aedis/tcp-echo-over-redis.png)

The tests were performed on a network where latency is 35ms on
average, otherwise it uses the same number of TCP connections
as the previous example.

As the reader can see, the Libuv and the Rust test are not depicted
in the graph, the reasons are

   * [redis-rs](https://github.com/redis-rs/redis-rs): This client
     comes so far behind that it can't even be represented together
     with the other benchmarks without making them look insignificant.
     I don't know for sure why it is so slow, I suppose it has
     something to do with its lack of proper
     [pipelining](https://redis.io/docs/manual/pipelining/) support.
     In fact, the more TCP connections I lauch the worse its
     performance gets.

   * Libuv: I left it out because it would require too much work to
     write it and make it have a good performance. More specifically,
     I would have to use hiredis and implement support for pipelines
     manually.

The code used in the benchmarks can be found at

   * [Aedis](https://github.com/mzimbres/aedis): [code](https://github.com/mzimbres/aedis/blob/3fb018ccc6138d310ac8b73540391cdd8f2fdad6/examples/echo_server.cpp)
   * [node-redis](https://github.com/redis/node-redis): [code](https://github.com/mzimbres/aedis/tree/3fb018ccc6138d310ac8b73540391cdd8f2fdad6/benchmarks/nodejs/echo_server_over_redis)
   * [go-redis](https://github.com/go-redis/redis): [code](https://github.com/mzimbres/aedis/blob/3fb018ccc6138d310ac8b73540391cdd8f2fdad6/benchmarks/go/echo_server_over_redis.go)

## Changelog

### v1.1.0

* Removes `coalesce_requests` from the `aedis::connection::config`, it
  became a request property now, see `aedis::resp3::request::config::coalesce`.

* Removes `max_read_size` from the `aedis::connection::config`. The maximum
  read size can be specified now as a parameter of the
  `aedis::adapt()` function.

* Removes `aedis::sync` class, see intro_sync.cpp for how to perform
  synchronous and thread safe calls. This is possible in Boost. 1.80
  only as it requires `boost::asio::deferred`. 

* Moves from `boost::optional` to `std::optional`. This is part of
  moving to C++17.

* Changes the behaviour of the second `aedis::connection::async_run` overload
  so that it always returns an error when the connection is lost.

* Adds TLS support, see intro_tls.cpp.

* Adds an example that shows how to resolve addresses over sentinels,
  see subscriber_sentinel.cpp.

* Adds a `aedis::connection::timeouts::resp3_handshake_timeout`. This is
  timeout used to send the `HELLO` command.

* Adds `aedis::endpoint` where in addition to host and port, users can
  optionally provide username, password and the expected server role
  (see `aedis::error::unexpected_server_role`).

* `aedis::connection::async_run` checks whether the server role received in
  the hello command is equal to the expected server role specified in
  `aedis::endpoint`. To skip this check let the role variable empty.

* Removes reconnect functionanlity from `aedis::connection`. It
  is possible in simple reconnection strategies but bloats the class
  in more complex scenarios, for example, with sentinel,
  authentication and TLS. This is trivial to implement in a separate
  coroutine. As a result the enum `event` and `async_receive_event`
  have been removed from the class too.

* Fixes a bug in `connection::async_receive_push` that prevented
  passing any response adapter other that `adapt(std::vector<node>)`.

* Changes the behaviour of `aedis::adapt()` that caused RESP3 errors
  to be ignored. One consequence of it is that `connection::async_run`
  would not exit with failure in servers that required authentication.

* Changes the behaviour of `connection::async_run` that would cause it
  to complete with success when an error in the
  `connection::async_exec` occurred.

* Ports the buildsystem from autotools to CMake.

### v1.0.0

* Adds experimental cmake support for windows users.

* Adds new class `aedis::sync` that wraps an `aedis::connection` in
  a thread-safe and synchronous API.  All free functions from the
  `sync.hpp` are now member functions of `aedis::sync`.

* Split `aedis::connection::async_receive_event` in two functions, one
  to receive events and another for server side pushes, see
  `aedis::connection::async_receive_push`.

* Removes collision between `aedis::adapter::adapt` and
  `aedis::adapt`.

* Adds `connection::operation` enum to replace `cancel_*` member
  functions with a single cancel function that gets the operations
  that should be cancelled as argument.

* Bugfix: a bug on reconnect from a state where the `connection` object
  had unsent commands. It could cause `async_exec` to never
  complete under certain conditions.

* Bugfix: Documentation of `adapt()` functions were missing from
  Doxygen.

### v0.3.0

* Adds `experimental::exec` and `receive_event` functions to offer a
  thread safe and synchronous way of executing requests across
  threads. See `intro_sync.cpp` and `subscriber_sync.cpp` for
  examples.

* `connection::async_read_push` was renamed to `async_receive_event`.

* `connection::async_receive_event` is now being used to communicate
  internal events to the user, such as resolve, connect, push etc. For
  examples see subscriber.cpp and `connection::event`.

* The `aedis` directory has been moved to `include` to look more
  similar to Boost libraries. Users should now replace `-I/aedis-path`
  with `-I/aedis-path/include` in the compiler flags.

* The `AUTH` and `HELLO` commands are now sent automatically. This change was
  necessary to implement reconnection. The username and password
  used in `AUTH` should be provided by the user on
  `connection::config`.

* Adds support for reconnection. See `connection::enable_reconnect`.

* Fixes a bug in the `connection::async_run(host, port)` overload
  that was causing crashes on reconnection.

* Fixes the executor usage in the connection class. Before theses
  changes it was imposing `any_io_executor` on users.

* `connection::async_receiver_event` is not cancelled anymore when
  `connection::async_run` exits. This change makes user code simpler.

* `connection::async_exec` with host and port overload has been
  removed. Use the other `connection::async_run` overload.

* The host and port parameters from `connection::async_run` have been
  move to `connection::config` to better support authentication and
  failover.

* Many simplifications in the `chat_room` example.

* Fixes build in clang the compilers and makes some improvements in
  the documentation.

### v0.2.1

* Fixes a bug that happens on very high load.

### v0.2.0

* Major rewrite of the high-level API. There is no more need to use the low-level API anymore.
* No more callbacks: Sending requests follows the ASIO asynchronous model.
* Support for reconnection: Pending requests are not canceled when a connection is lost and are re-sent when a new one is established.
* The library is not sending HELLO-3 on user behalf anymore. This is important to support AUTH properly.

### v0.1.2

* Adds reconnect coroutine in the `echo_server` example.
* Corrects `client::async_wait_for_data` with `make_parallel_group` to launch operation.
* Improvements in the documentation.
* Avoids dynamic memory allocation in the client class after reconnection.

### v0.1.1

* Improves the documentation and adds some features to the high-level client.

### v0.1.0

* Improvements in the design and documentation.

### v0.0.1

* First release to collect design feedback.

## Acknowledgement

Acknowldgement to people that helped shape Aedis in one way or
another.

* Richard Hodges ([madmongo1](https://github.com/madmongo1)): For very helpful support with Asio, the design of asynchronous programs, etc.
* Vinícius dos Santos Oliveira ([vinipsmaker](https://github.com/vinipsmaker)): For useful discussion about how Aedis consumes buffers in the read operation.
* Petr Dannhofer ([Eddie-cz](https://github.com/Eddie-cz)): For helping me understand how the `AUTH` and `HELLO` command can influence each other.
* Mohammad Nejati ([ashtum](https://github.com/ashtum)): For pointing scenarios where calls to `async_exec` should fail when the connection is lost.
* Klemens Morgenstern ([klemens-morgenstern](https://github.com/klemens-morgenstern)): For useful discussion about timeouts, the synchronous interface and general help with Asio.

