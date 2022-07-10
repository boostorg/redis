/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_HPP
#define AEDIS_HPP

#include <aedis/error.hpp>
#include <aedis/command.hpp>
#include <aedis/adapt.hpp>
#include <aedis/connection.hpp>
#include <aedis/resp3/read.hpp>
#include <aedis/resp3/write.hpp>
#include <aedis/resp3/exec.hpp>
#include <aedis/resp3/request.hpp>
#include <aedis/adapter/adapt.hpp>

// \li Support for Redis [sentinel](https://redis.io/docs/manual/sentinel).
// TODO: Reconnect support.
// TODO: Remove conflicts of the adapt function.

/** \mainpage Documentation
    \tableofcontents
  
    \section Overview
  
    Aedis is a high-level [Redis](https://redis.io/) client library
    built on top of [Asio](https://www.boost.org/doc/libs/release/doc/html/boost_asio.html)
    that provides simple and efficient communication with a Redis
    server. Some of its distinctive features are

    \li Support for the latest version of the Redis communication protocol [RESP3](https://github.com/redis/redis-specifications/blob/master/protocol/RESP3.md).
    \li First class support for STL containers and C++ built-in types.
    \li Serialization and deserialization of your own data types.
    \li Zero asymptotic allocations by means of memory reuse.
    \li Healthy checks, back pressure and low latency.

    Aedis API hides most of the low level asynchronous operations
    away from the user, for example, the code below pings a message to
    the server

    \code
    int main()
    {
       request req;
       req.push("HELLO", 3);
       req.push("PING");
       req.push("QUIT");

       std::tuple<aedis::ignore, std::string, aedis::ignore> resp;

       net::io_context ioc;

       connection db{ioc};

       db.async_exec("127.0.0.1", "6379", req, adapt(resp),
          [](auto ec, auto) { std::cout << ec.message() << std::endl; });

       ioc.run();

       // Print the ping message.
       std::cout << std::get<1>(resp) << std::endl;
    }
    \endcode

    For a detailed comparison of Redis clients and the design
    rationale behind Aedis jump to \ref why-aedis.

    \section requests Requests

    Redis requests are composed of one of more Redis commands (in
    Redis documentation they are called
    [pipelines](https://redis.io/topics/pipelining)). For example

    @code
    request req;

    // Command with variable length of arguments.
    req.push("SET", "key", "some value", value, "EX", "2");

    // Pushes a set.
    std::list<std::string> list
       {"channel1", "channel2", "channel3"};
    req.push_range("SUBSCRIBE", list);

    // Same as above but as an iterator range.
    req.push_range2("SUBSCRIBE", std::cbegin(list), std::cend(list));

    // Sends a map.
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

    The second argument \c adapt(resp) will be explained in \ref requests.

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
       // Convert to obj string and call to_bulk.
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

    It is quite common to store json string in Redis for example.

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

    RESP3 type     | C++                                                          | Type
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

    Responses that contain nested aggregates or heterogeneous data
    types will be given special treatment later in \ref gen-case.  As
    of this writing, not all RESP3 types are used by the Redis server,
    which means in practice users will be concerned with a reduced
    subset of the RESP3 specification. Now let us see some examples

    @code
    auto dbuffer = dynamic_buffer(buffer);

    // To ignore the response.
    co_await resp3::async_read(socket, dbuffer, adapt());

    // Read in a std::string e.g. get.
    std::string str;
    co_await resp3::async_read(socket, dbuffer, adapt(str));

    // Read in a long long e.g. rpush.
    long long number;
    co_await resp3::async_read(socket, dbuffer, adapt(number));

    // Read in a std::set e.g. smembers.
    std::set<T, U> set;
    co_await resp3::async_read(socket, dbuffer, adapt(set));

    // Read in a std::map e.g. hgetall.
    std::map<T, U> set;
    co_await resp3::async_read(socket, dbuffer, adapt(map));

    // Read in a std::unordered_map e.g. hgetall.
    std::unordered_map<T, U> umap;
    co_await resp3::async_read(socket, dbuffer, adapt(umap));

    // Read in a std::vector e.g. lrange.
    std::vector<T> vec;
    co_await resp3::async_read(socket, dbuffer, adapt(vec));
    @endcode

    In other words, it is straightforward, just pass the result of \c
    adapt to the read function and make sure the response data type is
    compatible with the data structure you are calling @c adapter(...)
    with. All standard C++ containers are supported by Aedis.

    \subsection Optional

    It is not uncommon for apps to access keys that do not exist or
    that have already expired in the Redis server, to deal with these
    cases Aedis provides support for \c boost::optional. To use it,
    wrap your type around \c boost::optional like this

    @code
    boost::optional<std::unordered_map<T, U>> umap;
    co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt(umap));
    @endcode

    Everything else stays the same, before accessing data, users will
    have to check or assert the optional contains a value.

    \subsection heterogeneous_aggregates Heterogeneous aggregates

    There are cases where Redis returns aggregates that
    contain heterogeneous data, for example, an array that contains
    integers, strings nested sets etc. Aedis supports reading such
    aggregates in a \c std::tuple efficiently as long as the they
    don't contain 3-order nested aggregates e.g. an array that
    contains an array of arrays. For example, to read the response to
    a \c hello command we can use the following response type.

    @code
    using hello_type = std::tuple<
       std::string, std::string,
       std::string, std::string,
       std::string, int,
       std::string, int,
       std::string, std::string,
       std::string, std::string,
       std::string, std::vector<std::string>>;
    @endcode

    Transactions are another example where this feature is useful, for
    example, the response to the transaction below

    @code
    db.send("MULTI");
    db.send("GET", "key1");
    db.send("LRANGE", "key2", 0, -1);
    db.send("HGETALL", "key3");
    db.send("EXEC");
    @endcode

    can be read in the following way

    @code
    std::tuple<
       boost::optional<std::string>, // Response to get
       boost::optional<std::vector<std::string>>, // Response to lrange
       boost::optional<std::map<std::string, std::string>> // Response to hgetall
    > trans;

    co_await resp3::async_read(socket, dynamic_buffer(buffer)); // Ignore multi
    co_await resp3::async_read(socket, dynamic_buffer(buffer)); // Ignore get
    co_await resp3::async_read(socket, dynamic_buffer(buffer)); // Ignore lrange
    co_await resp3::async_read(socket, dynamic_buffer(buffer)); // Ignore hgetall
    co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt(trans));
    @endcode

    Note that above we are not ignoring the response to the commands
    themselves but whether they have been successfully queued.  Only
    after @c exec is received Redis will execute them in sequence and
    send all responses together in an array.

    \subsection Serialization

    As mentioned in \ref requests-serialization, it is common for
    users to serialized data before sending it to Redis e.g.  json
    strings, for example

    @code
    sr.push("SET", "key", "{"Server": "Redis"}"); // Unquoted for readability.
    sr.push("GET", "key")
    @endcode

    For performance and convenience reasons, we may want to avoid
    receiving the response to the \c get command above as a string
    just to convert it later to a e.g. deserialized json. To support
    this, Aedis calls a user defined \c from_bulk function while
    parsing the response. In simple terms, define your type

    @code
    struct mystruct {
       // struct fields.
    };
    @endcode

    and deserialize it from a string in a function \c from_bulk with
    the following signature

    @code
    void from_bulk(mystruct& obj, char const* p, std::size_t size, boost::system::error_code& ec)
    {
       // Deserializes p into obj.
    }
    @endcode

    After that, you can start receiving data efficiently in the desired
    types e.g. \c mystruct, \c std::map<std::string, mystruct> etc.

    \subsection gen-case The general case

    As already mentioned, there are cases where responses to Redis
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
    Using it is no different that using other types

    @code
    // Receives any RESP3 simple data type.
    node<std::string> resp;
    co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt(resp));

    // Receives any RESP3 simple or aggregate data type.
    std::vector<node<std::string>> resp;
    co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt(resp));
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
    @li containers.cpp: Shows how to send and receive stl containers.
    @li serialization.cpp: Shows the \c request support to serialization of user types.
    @li subscriber.cpp: Shows how channel subscription works.
    @li echo_server.cpp: A simple TCP echo server that users coroutines.
    @li chat_room.cpp: A simple chat room that uses coroutines.

    \section using-aedis Using Aedis

    To install and use Aedis you will need
  
    - Boost 1.78 or greater.
    - Unix Shell and Make (for linux users).
    - C++14. Some examples require C++20 with coroutine support.
    - Redis server.

    Some examples will also require interaction with
  
    - redis-cli: Used in one example.
    - Redis Sentinel Server: used in some examples.

    Aedis has been tested with the following compilers

    - Tested with gcc: 7.5.0, 8.4.0, 9.3.0, 10.3.0.
    - Tested with clang: 11.0.0, 10.0.0, 9.0.1, 8.0.1, 7.0.1.
  
    \section Installation

    The first thing to do is to download and unpack Aedis

    ```
    # Download the latest release on github
    $ wget https://github.com/mzimbres/aedis/releases
  
    # Uncompress the tarball and cd into the dir
    $ tar -xzvf aedis-version.tar.gz
    ```

    If you can't use \c configure and \c make (e.g. Windows users)
    add the directory where you unpacked Aedis to the
    include directories in your project, otherwise run
  
    ```
    # See configure --help for all options.
    $ ./configure --prefix=/opt/aedis-version --with-boost=/opt/boost_1_78_0

    # Install Aedis in the path specified in --prefix
    $ sudo make install
  
    ```
  
    and include the following header 
  
    ```cpp
    #include <aedis/src.hpp>

    ```

    in exactly one source file in your applications. At this point you
    can start using Aedis. To build the examples and run the tests run
  
    ```
    # Build aedis examples.
    $ make examples
  
    # Test aedis in your machine.
    $ make check
    ```

    \section Developers
  
    To generate the build system run
  
    ```
    $ autoreconf -i
    ```
  
    After that you will have a configure script 
    that you can run as explained above, for example, to use a
    compiler other that the system compiler run
  
    ```
    $ CC=/opt/gcc-10.2.0/bin/gcc-10.2.0 CXX=/opt/gcc-10.2.0/bin/g++-10.2.0 CXXFLAGS="-g -Wall -Werror"  ./configure ...
    $ make distcheck
    ```

    \section why-aedis Why Aedis

    At the time of this writing there are seventeen Redis clients
    listed in the [official](https://redis.io/docs/clients/#cpp) list.
    With so many clients available it is not unlikely that users are
    asking themselves why yet another one.  In this section I will try
    to compare Aedis to the most popular clients and why we need
    Aedis. Notice however that this is ongoing work as comparing
    client objectively is difficult and time consuming.

    The most popular client at the moment of this writing ranked by
    github stars is

    @li https://github.com/sewenew/redis-plus-plus

    Before we start it is worth mentioning some of the things it does
    not support

    @li RESP3. Without RESP3 is impossible to support some important Redis features like client side caching, among other things.
    @li The Asio asynchronous model.
    @li Reading response diretly in user data structures avoiding temporaries.
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

    @li Heterogeneous treatment of commands, pipelines and transaction.
    @li Any Api that sends individual commands has a very restricted scope of usability and should be avoided for performance reasons.
    @li The API imposes exceptions on users, no error-code overload is provided.
    @li No way to reuse the buffer for new calls to e.g. \c redis.get in order to avoid further dynamic memory allocations.
    @li Error handling of resolve and connection no clear.

    According to the documentation, pipelines in redis-plus-plus have
    the following characteristics

    > NOTE: By default, creating a Pipeline object is NOT cheap, since
    > it creates a new connection.

    This is clearly a downside of the API as pipelines should be the
    default way of communicating and not an exception, paying such a
    high price for each pipeline imposes a severe cost in performance.
    Transactions also suffer from the very same problem

    > NOTE: Creating a Transaction object is NOT cheap, since it
    > creates a new connection.

    In Aedis there is no difference between sending one command, a
    pipeline or a transaction because creating the request is decoupled
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

    \section Reference
  
    See \subpage any.

 */

/** \defgroup any Reference
 *
 *  This page contains the documentation of all user facing code.
 */

#endif // AEDIS_HPP
