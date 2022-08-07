/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_HPP
#define AEDIS_HPP

#include <aedis/error.hpp>
#include <aedis/adapt.hpp>
#include <aedis/connection.hpp>
#include <aedis/resp3/request.hpp>

/** \mainpage Documentation
    \tableofcontents
  
    Useful links: \subpage any, [Changelog](CHANGELOG.md) and [Benchmarks](benchmarks/benchmarks.md).

    \section Overview
  
    Aedis is a high-level [Redis](https://redis.io/) client library
    built on top of
    [Asio](https://www.boost.org/doc/libs/release/doc/html/boost_asio.html),
    some of its distinctive features are

    \li Support for the latest version of the Redis communication protocol [RESP3](https://github.com/redis/redis-specifications/blob/master/protocol/RESP3.md).
    \li First class support for STL containers and C++ built-in types.
    \li Serialization and deserialization of your own data types.
    \li Healthy checks, back pressure and low latency.
    \li Hides most of the low level asynchronous operations away from the user.

    Let us start with an overview of asynchronous code.

    @subsection Async

    The code below sends a ping command to Redis (see intro.cpp)

    @code
    int main()
    {
       net::io_context ioc;
       connection db{ioc};

       request req;
       req.push("PING");
       req.push("QUIT");

       std::tuple<std::string, aedis::ignore> resp;
       db.async_run(req, adapt(resp), net::detached);

       ioc.run();

       std::cout << std::get<0>(resp) << std::endl;
    }
    @endcode

    The connection class maintains a healthy connection with
    Redis over which users can execute their commands, without any
    need of queuing. For example, to execute more than one command

    @code
    int main()
    {
       ...
       net::io_context ioc;
       connection db{ioc};

       db.async_exec(req1, adapt(resp1), handler1);
       db.async_exec(req2, adapt(resp2), handler2);
       db.async_exec(req3, adapt(resp3), handler3);

       db.async_run(net::detached);

       ioc.run();
       ...
    }
    @endcode

    The `async_exec` functions above can be called from different
    places in the code without knowing about each other, see for
    example echo_server.cpp.  Server-side pushes are supported on the
    same connection where commands are executed, a typical subscriber
    will look like
    (see subscriber.cpp)

    @code
    net::awaitable<void> reader(std::shared_ptr<connection> db)
    {
       request req;
       req.push("SUBSCRIBE", "channel");

       for (std::vector<node_type> resp;;) {
          auto ev = co_await db->async_receive_event(aedis::adapt(resp));

          switch (ev) {
             case connection::event::push:
             // Use resp.
             resp.clear();
             break;

             case connection::event::hello:
             // Subscribes to channels when a new connection is
             // stablished.
             co_await db->async_exec(req);
             break;

             default:;
          }
       }
    }
    @endcode

    @subsection Sync

    The `connection` class is async-only, many users however need to
    interact with it synchronously, this is also supported by Aedis as long
    as this interaction occurs across threads, for example (see
    intro_sync.cpp)

    @code
    int main()
    {
       try {
          net::io_context ioc{1};
          connection conn{ioc};
    
          std::thread thread{[&]() {
             conn.async_run(net::detached);
             ioc.run();
          }};
    
          request req;
          req.push("PING");
          req.push("QUIT");
    
          std::tuple<std::string, aedis::ignore> resp;
          exec(conn, req, adapt(resp));
          thread.join();
    
          std::cout << "Response: " << std::get<0>(resp) << std::endl;
       } catch (std::exception const& e) {
          std::cerr << e.what() << std::endl;
       }
    }
    @endcode

    \subsection using-aedis Installation

    To install and use Aedis you will need
  
    - Boost 1.78 or greater.
    - C++17. Some examples require C++20 with coroutine support.
    - Redis 6 or higher. Optionally also redis-cli and Redis Sentinel.

    For a simple installation run

    ```
    # Clone the repository and checkout the lastest release tag.
    $ git clone --branch v0.3.0 https://github.com/mzimbres/aedis.git
    $ cd aedis

    # Build an example
    $ g++ -std=c++17 -pthread examples/intro.cpp -I./include -I/path/boost_1_79_0/include/
    ```

    For a proper full installation on the system run

    ```
    # Download and unpack the latest release
    $ wget https://github.com/mzimbres/aedis/releases/download/v0.3.0/aedis-0.2.1.tar.gz
    $ tar -xzvf aedis-0.2.1.tar.gz

    # Configure, build and install
    $ ./configure --prefix=/opt/aedis-0.2.1 --with-boost=/opt/boost_1_78_0
    $ sudo make install
    ```

    To build examples and tests

    ```
    $ make
    ```

    @subsubsection using_aedis Using Aedis

    When writing you own applications include the following header 
  
    ```cpp
    #include <aedis/src.hpp>

    ```

    in no more than one source file in your applications.

    @subsubsection sup-comp Supported compilers

    Aedis has been tested with the following compilers

    - Tested with gcc: 12, 11.
    - Tested with clang: 14, 13, 11.
  
    \subsubsection Developers
  
    To generate the build system clone the repository and run
  
    ```
    # git clone https://github.com/mzimbres/aedis.git
    $ autoreconf -i
    ```
  
    After that you will have a configure script that you can run as
    explained above, for example, to use a compiler other that the
    system compiler run
  
    ```
    $ CXX=clang++-14 CXXFLAGS="-g" ./configure --with-boost=...
    ```

    To generate release tarballs run

    ```
    $ make distcheck
    ```


    \section requests Requests

    Redis requests are composed of one of more Redis commands (in
    Redis documentation they are called
    [pipelines](https://redis.io/topics/pipelining)). For example

    @code
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
    @endcode

    Sending a request to Redis is then peformed with the following function

    @code
    co_await db->async_exec(req, adapt(resp));
    @endcode

    \subsection requests-serialization Serialization

    The \c push and \c push_range functions above work with integers
    e.g. \c int and \c std::string out of the box. To send your own
    data type defined a \c to_bulk function like this

    @code
    struct mystruct {
       // Example struct.
    };
    
    void to_bulk(std::string& to, mystruct const& obj)
    {
       std::string dummy = "Dummy serializaiton string.";
       aedis::resp3::to_bulk(to, dummy);
    }
    @endcode

    Once \c to_bulk is defined and accessible over ADL \c mystruct can
    be passed to the \c request

    @code
    request req;

    std::map<std::string, mystruct> map {...};

    req.push_range("HSET", "key", map);
    @endcode

    Example serialization.cpp shows how store json string in Redis.

    \section low-level-responses Responses

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
    Simple-string  | \c std::string                                             | Simple
    Simple-error   | \c std::string                                             | Simple
    Blob-string    | \c std::string, \c std::vector                             | Simple
    Blob-error     | \c std::string, \c std::vector                             | Simple
    Number         | `long long`, `int`, `std::size_t`, \c std::string          | Simple
    Double         | `double`, \c std::string                                   | Simple
    Null           | `boost::optional<T>`                                       | Simple
    Array          | \c std::vector, \c std::list, \c std::array, \c std::deque | Aggregate
    Map            | \c std::vector, \c std::map, \c std::unordered_map         | Aggregate
    Set            | \c std::vector, \c std::set, \c std::unordered_set         | Aggregate
    Push           | \c std::vector, \c std::map, \c std::unordered_map         | Aggregate

    For example

    @code
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
    @endcode

    The tag @c aedis::ignore can be used to ignore individual
    elements in the responses. If the intention is to ignore the
    response to all commands in the request use @c adapt()

    @code
    co_await db->async_exec(req, adapt());
    @endcode

    Responses that contain nested aggregates or heterogeneous data
    types will be given special treatment later in \ref gen-case.  As
    of this writing, not all RESP3 types are used by the Redis server,
    which means in practice users will be concerned with a reduced
    subset of the RESP3 specification.

    \subsection Optional

    It is not uncommon for apps to access keys that do not exist or
    that have already expired in the Redis server, to deal with these
    cases Aedis provides support for \c boost::optional. To use it,
    wrap your type around \c boost::optional like this

    @code
    boost::optional<std::unordered_map<T, U>> resp;
    co_await db->async_exec(req, adapt(resp));
    @endcode

    Everything else stays the same.

    \subsection transactions Transactions

    To read the response to transactions we have to observe that Redis
    queues the commands as they arrive and sends the responses back to
    the user in a single array, in the response to the @c exec command.
    For example, to read the response to the this request

    @code
    db.send("MULTI");
    db.send("GET", "key1");
    db.send("LRANGE", "key2", 0, -1);
    db.send("HGETALL", "key3");
    db.send("EXEC");
    @endcode

    use the following response type

    @code
    using aedis::ignore;
    using boost::optional;

    using tresp_type = 
       std::tuple<
          optional<std::string>, // get
          optional<std::vector<std::string>>, // lrange
          optional<std::map<std::string, std::string>> // hgetall
       >;

    std::tuple<
       ignore,     // multi
       ignore,     // get
       ignore,     // lrange
       ignore,     // hgetall
       tresp_type, // exec
    > resp;

    co_await db->async_exec(req, adapt(resp));
    @endcode

    Note that above we are not ignoring the response to the commands
    themselves but whether they have been successfully queued. For a
    complete example see containers.cpp.

    \subsection Deserialization

    As mentioned in \ref requests-serialization, it is common to
    serialize data before sending it to Redis e.g.  to json strings.
    For performance and convenience reasons, we may also want to
    deserialize it directly in its final data structure. Aedis
    supports this use case by calling a user provided \c from_bulk
    function while parsing the response. For example

    @code
    void from_bulk(mystruct& obj, char const* p, std::size_t size, boost::system::error_code& ec)
    {
       // Deserializes p into obj.
    }
    @endcode

    After that, you can start receiving data efficiently in the desired
    types e.g. \c mystruct, \c std::map<std::string, mystruct> etc.

    \subsection gen-case The general case

    There are cases where responses to Redis
    commands won't fit in the model presented above, some examples are

    @li Commands (like \c set) whose response don't have a fixed
    RESP3 type. Expecting an \c int and receiving a blob-string
    will result in error.
    @li RESP3 aggregates that contain nested aggregates can't be read in STL containers.
    @li Transactions with a dynamic number of commands can't be read in a \c std::tuple.

    To deal with these cases Aedis provides the \c resp3::node
    type, that is the most general form of an element in a response,
    be it a simple RESP3 type or an aggregate. It is defined like this

    @code
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
    @endcode

    Any response to a Redis command can be received in a \c
    std::vector<node<std::string>>.  The vector can be seen as a
    pre-order view of the response tree
    (https://en.wikipedia.org/wiki/Tree_traversal#Pre-order,_NLR).
    Using it is no different than using other types

    @code
    // Receives any RESP3 simple data type.
    node<std::string> resp;
    co_await db->async_exec(req, adapt(resp));

    // Receives any RESP3 simple or aggregate data type.
    std::vector<node<std::string>> resp;
    co_await db->async_exec(req, adapt(resp));
    @endcode

    For example, suppose we want to retrieve a hash data structure
    from Redis with \c hgetall, some of the options are

    @li \c std::vector<node<std::string>: Works always.
    @li \c std::vector<std::string>: Efficient and flat, all elements as string.
    @li \c std::map<std::string, std::string>: Efficient if you need the data as a \c std::map
    @li \c std::map<U, V>: Efficient if you are storing serialized data. Avoids temporaries and requires \c from_bulk for \c U and \c V.

    In addition to the above users can also use unordered versions of the containers. The same reasoning also applies to sets e.g. \c smembers.

    \section examples Examples

    The examples listed below cover most use cases presented in the documentation above.

    @li intro.cpp: Basic steps with Aedis.
    @li intro_sync.cpp: Synchronous version of intro.cpp.
    @li containers.cpp: Shows how to send and receive stl containers.
    @li serialization.cpp: Shows the \c request support to serialization of user types.
    @li subscriber.cpp: Shows how to subscribe to a channel and how to reconnect when connection is lost.
    @li subscriber_sync.cpp: Synchronous version of subscriber.cpp.
    @li echo_server.cpp: A simple TCP echo server that uses coroutines.
    @li chat_room.cpp: A simple chat room that uses coroutines.

    \section why-aedis Why Aedis

    At the time of this writing there are seventeen Redis clients
    listed in the [official](https://redis.io/docs/clients/#cpp) list.
    With so many clients available it is not unlikely that users are
    asking themselves why yet another one.  In this section I will try
    to compare Aedis with the most popular clients and why we need
    Aedis. Notice however that this is ongoing work as comparing
    client objectively is difficult and time consuming.

    The most popular client at the moment of this writing ranked by
    github stars is

    @li https://github.com/sewenew/redis-plus-plus

    Before we start it is worth mentioning some of the things it does
    not support

    @li RESP3. Without RESP3 is impossible to support some important Redis features like client side caching, among other things.
    @li Coroutines.
    @li Reading responses directly in user data structures avoiding temporaries.
    @li Error handling with error-code and exception overloads.
    @li Healthy checks.

    The remaining points will be addressed individually.

    @subsection redis-plus-plus

    Let us first have a look at what sending a command a pipeline and a
    transaction look like

    @code
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
    @endcode

    Some of the problems with this API are

    @li Heterogeneous treatment of commands, pipelines and transaction. This makes auto-pipelining impossible.
    @li Any Api that sends individual commands has a very restricted scope of usability and should be avoided for performance reasons.
    @li The API imposes exceptions on users, no error-code overload is provided.
    @li No way to reuse the buffer for new calls to e.g. \c redis.get in order to avoid further dynamic memory allocations.
    @li Error handling of resolve and connection not clear.

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

    @code
    auto async_redis = AsyncRedis(opts, pool_opts);
    
    Future<string> ping_res = async_redis.ping();
    
    cout << ping_res.get() << endl;
    @endcode

    As the reader can see, the async interface is based on futures
    which is also known to have a bad performance.  The biggest
    problem however with this async design is that it makes it
    impossible to write asynchronous programs correctly since it
    starts an async operation on every command sent instead of
    enqueueing a message and triggering a write when it can be sent.
    It is also not clear how are pipelines realised with the design
    (if at all).

    \section Acknowledgement

    Some people that were helpful in the development of Aedis

    @li Richard Hodges ([madmongo1](https://github.com/madmongo1)): For helping me with Asio and the design of asynchronous programs in general.
    @li Vinícius dos Santos Oliveira ([vinipsmaker](https://github.com/vinipsmaker)): For useful discussion about how Aedis consumes buffers in the read operation (among other things).
    @li Petr Dannhofer ([Eddie-cz](https://github.com/Eddie-cz)): For helping me understand how the `AUTH` and `HELLO` command can influence each other.
 */

/** \defgroup any Reference
 *
 *  This page contains the documentation of all user facing code.
 */

//  Support sentinel support as described in 
//
//  - https://redis.io/docs/manual/sentinel.
//  - https://redis.io/docs/reference/sentinel-clients.
//
//  Avoid conflicts between
//
//  - aedis::adapt 
//  - aedis::resp3::adapt.

#endif // AEDIS_HPP
