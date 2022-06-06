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

/** \mainpage Documentation
    \tableofcontents
  
    \section Overview
  
    Aedis is a high-level [Redis](https://redis.io/) client library built on top
    of [Asio](https://www.boost.org/doc/libs/release/doc/html/boost_asio.html)
    that provides simple and efficient communication with a Redis
    server. Some of its distinctive features are

    @li Support for the latest version of the Redis communication protocol [RESP3](https://github.com/redis/redis-specifications/blob/master/protocol/RESP3.md).
    @li First class support for STL containers and C++ built-in types.
    @li Serialization and deserialization of your own data types that avoid unnecessary copies.
    @li Support for Redis [sentinel](https://redis.io/docs/manual/sentinel).

    In addition to that, Aedis provides a high-level client that
    offers the following functionality

    @li Management of message queues.
    @li Simplified handling of server pushes.
    @li Zero asymptotic allocations by means of memory reuse.
    @li Healthy checks.
    @li Back pressure.

    If you are interested in a detailed comparison of Redis clients
    and the design rationale behind Aedis jump to \ref why-aedis. Now
    let us have a look at the low-level API.

    \section low-level-api Low-level API

    The low-level API is very useful for tasks that can be performed
    in short lived connections, for example, assume we want to perform
    the following steps

    @li Set the value of a Redis key.
    @li Set the expiration of that key to two seconds.
    @li Get and return its old value.
    @li Quit

    The coroutine-based asynchronous implementation of the steps above look like

    @code
    net::awaitable<std::string> set(net::ip::tcp::endpoint ep)
    {
       // To make code less verbose
       using tcp_socket = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::socket>;

       tcp_socket socket{co_await net::this_coro::executor};
       co_await socket.async_connect(ep);
    
       std::string buffer, response;

       auto sr = make_serializer(request);
       sr.push(command::hello, 3);
       sr.push(command::set, "key", "Value", "EX", "2", "get");
       sr.push(command::quit);
       co_await net::async_write(socket, net::buffer(buffer));
       buffer.clear();
    
       auto dbuffer = net::dynamic_buffer(read_buffer);
       co_await resp3::async_read(socket, dbuffer); // Hello ignored.
       co_await resp3::async_read(socket, dbuffer, adapt(response)); // Set
       co_await resp3::async_read(socket, dbuffer); // Quit ignored.
    
       co_return response;
    }
    @endcode

    The simplicity of the code above makes it self explanatory

    @li Connect to the Redis server.
    @li Declare a \c std::string to hold the request and add some commands in it with a serializer.
    @li Write the payload to the socket and read the responses in the same order they were sent.
    @li Return the response to the user.

    The @c hello command above is always required and must be sent
    first as it informs we want to communicate over RESP3.

    \subsection requests Requests

    As stated above, requests are created by defining a storage object
    and a serializer that knows how to convert user data into valid
    RESP3 wire-format. They are composed of one or more
    commands (in Redis documentation they are called [pipelines](https://redis.io/topics/pipelining)),
    which means users can add
    as many commands to the request as they like, a feature that aids
    performance.

    The individual commands in a request assume many
    different forms

    @li With and without keys.
    @li Variable length arguments.
    @li Ranges.
    @li etc.

    To account for all these variations, the \c serializer class
    offers some member functions, each of them with a couple of
    overloads, for example

    @code
    // Some data to send to Redis.
    std::string value = "some value";

    std::list<std::string> list {"channel1", "channel2", "channel3"};

    std::map<std::string, mystruct> map
       { {"key1", "value1"}
       , {"key2", "value2"}
       , {"key3", "value3"}};

    // Command with no arguments
    sr.push(command::quit);

    // Command with variable lenght arguments.
    sr.push(command::set, "key", value, "EX", "2");

    // Sends a container, no key.
    sr.push_range(command::subscribe, list);

    // Same as above but an iterator range.
    sr.push_range2(command::subscribe, std::cbegin(list), std::cend(list));

    // Sends a container, with key.
    sr.push_range(command::hset, "key", map);

    // Same as above but as iterator range.
    sr.push_range2(command::hset, "key", std::cbegin(map), std::cend(map));
    @endcode

    Once all commands have been added to the request, we can write it
    as usual by writing the payload to the socket

    @code
    co_await net::async_write(socket, buffer(request));
    @endcode

    \subsubsection requests-serialization Serialization

    The \c send and \c send_range functions above work with integers
    e.g. \c int and \c std::string out of the box. To send your own
    data type defined the \c to_bulk function like this

    @code
    // Example struct.
    struct mystruct {
      // ...
    };
    
    void to_bulk(std::string& to, mystruct const& obj)
    {
       // Convert to obj string and call to_bulk (see also add_header
       // and add_separator)
       auto dummy = "Dummy serializaiton string.";
       aedis::resp3::to_bulk(to, dummy);
    }

    std::map<std::string, mystruct> map
       { {"key1", {...}}
       , {"key2", {...}}
       , {"key3", {...}}};

    db.send_range(command::hset, "key", map);
    @endcode

    It is quite common to store json string in Redis for example.

    \subsection low-level-responses Responses

    To read responses effectively, users must know their RESP3 type,
    this can be found in the Redis documentation of each command
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
    types will be given special treatment later.  As of this writing,
    not all RESP3 types are used by the Redis server, which means in
    practice users will be concerned with a reduced subset of the
    RESP3 specification. Now let us see some examples

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

    \subsubsection Optional

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

    \subsubsection heterogeneous_aggregates Heterogeneous aggregates

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
    db.send(command::multi);
    db.send(command::get, "key1");
    db.send(command::lrange, "key2", 0, -1);
    db.send(command::hgetall, "key3");
    db.send(command::exec);
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

    \subsubsection Serialization

    As mentioned in \ref requests-serialization, it is common for
    users to serialized data before sending it to Redis e.g.  json
    strings, for example

    @code
    sr.push(command::set, "key", "{"Server": "Redis"}"); // Unquoted for readability.
    sr.push(command::get, "key")
    @endcode

    For performance and convenience reasons, we may want to avoid
    receiving the response to the \c get command above as a string
    just to convert it later to a e.g. deserialized json. To support
    this, Aedis calls a user defined \c from_string function while
    parsing the response. In simple terms, define your type

    @code
    struct mystruct {
       // struct fields.
    };
    @endcode

    and deserialize it from a string in a function \c from_string with
    the following signature

    @code
    void from_string(mystruct& obj, char const* p, std::size_t size, boost::system::error_code& ec)
    {
       // Deserializes p into obj.
    }
    @endcode

    After that, you can start receiving data efficiently in the desired
    types e.g. \c mystruct, \c std::map<std::string, mystruct> etc.

    \subsubsection gen-case The general case

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
    @li \c std::map<U, V>: Efficient if you are storing serialized data. Avoids temporaries and requires \c from_string for \c U and \c V.

    In addition to the above users can also use unordered versions of the containers. The same reasoning also applies to sets e.g. \c smembers.

    \subsubsection low-level-adapters Adapters

    Users that are not satisfied with any of the options above can
    write their own adapters very easily. For example, the adapter below
    can be used to print incoming data to the screen.

    @code
    auto adapter = [](resp3::node<boost::string_view> const& nd, boost::system::error_code&)
    {
       std::cout << nd << std::endl;
    };

    co_await resp3::async_read(socket, dynamic_buffer(buffer), adapter);
    @endcode

    See more in the \ref examples section.

    \section high-level-api High-level API

    As stated earlier, the low-level API is very useful for tasks that
    can be performed with short lived connections. Sometimes however,
    the need for long-lived connections becomes compeling

    @li \b Server \b pushes: Short lived connections can't deal with server pushes, that means no [client side caching](https://redis.io/topics/client-side-caching), [notifications](https://redis.io/topics/notifications) and [pubsub](https://redis.io/topics/pubsub).
    @li \b Performance: Keep opening and closing connections impact performance serverely.
    @li \b Pipeline: Code such as shown in \ref low-level-api don't support pipelines well since it can only send a fixed number of commands at time. It misses important optimization opportunities (https://redis.io/topics/pipelining).

    A serious implementation that supports the points listed above is
    far from trivial and involves many complex asynchronous operations

    @li \c async_resolve: Resolve a hostname.
    @li \c async_connect: Connect to Redis.
    @li \c async_read: Performed in a loop as long as the connection lives.
    @li \c async_write: Performed everytime a new message is added.
    @li \c async_wait: To timout all operations above if the server becomes unresponsive.

    Notice that many of the operations above will run concurrently
    with each other and, in addition to that

    @li \c async_write operations require management of the message queue to prevent concurrent writes.
    @li Healthy checks must be sent periodically by the client to detect a dead or unresponsive server.
    @li Recovery after a disconnection to avoid loosing enqueued commands.
    @li Back pressure.

    Expecting users to implement these points themselves is
    unrealistic and could result in code that performs poorly and
    can't handle errors properly.  To avoid all of that, Aedis
    provides its own implementation. The general form of a program
    that uses the high-level API looks like this

    @code
    int main()
    {
       net::io_context ioc;

       client_type db(ioc.get_executor());
       auto recv = std::make_shared<receiver>(db);
       db.set_receiver(recv);

       db.async_run([](auto ec){ ... });

       ioc.run();
    }
    @endcode

    Users are concerned only with the implementation of the
    receiver. For example

    @code
    // Callbacks.
    struct receiver {
       void on_resp3(command cmd, node<string_view> const& nd, error_code& ec) { ...  }
       void on_read(command cmd, std::size_t) { ...  }
    };
    @endcode

    The functions in the receiver above are callbacks that will be
    called when events arrives

    @li \c on_resp3: Called when a new chunk of resp3 data is parsed.
    @li \c on_read: Called after the response to a command has been successfully read.

    The callbacks above are never called on errors, instead the \c
    async_run function returns. Reconnection is also supported, for
    example

    @code
    net::awaitable<void> run(std::shared_ptr<client_type> db)
    {
       auto ex = co_await net::this_coro::executor;
    
       boost::asio::steady_timer timer{ex};
    
       for (error_code ec;;) {
          co_await db->async_run(redirect_error(use_awaitable, ec));

          // Log the error.
          std::clog << ec.message() << std::endl;
    
          // Wait two seconds and try again.
          timer.expires_after(std::chrono::seconds{2});
          co_await timer.async_wait(redirect_error(use_awaitable, ec));
       }
    }
    @endcode

    when reconnecting the client will recover requests that haven't
    been sent to Redis yet.

    \subsection high-level-sending-cmds Sending commands

    The db object from the example above can be passed around to other
    objects so that commands can be sent from everywhere in the app.
    Sending commands is also similar to what has been discussed before

    @code
    void foo(client<net::ip::tcp::socket>& db)
    {
       db.send(command::ping, "O rato roeu a roupa do rei de Roma");
       db.send(command::incr, "counter");
       db.send(command::set, "key", "Três pratos de trigo para três tigres");
       db.send(command::get, "key");
       ...
    }
    @endcode

    The \c send functions in this case will add commands to the output
    queue and send them only if there is no pending response. This is
    so because RESP3 is a request/response protocol, which means
    clients must wait for responses before sending
    the next request.

    \section examples Examples

    To better fix what has been said above, users should have a look at some simple examples.

    \b Low \b level \b API (sync)

    @li intro_sync.cpp: Synchronous API usage example.
    @li serialization_sync.cpp: Shows how serialize your own types.

    \b Low \b level \b API (async-coroutine)

    @li subscriber.cpp: Shows how channel subscription works at the low level.
    @li transaction.cpp: Shows how to read the response to transactions.
    @li custom_adapter.cpp: Shows how to write a response adapter that prints to the screen, see \ref low-level-adapters.

    \b High \b level \b API (async only)

    @li intro_high_level.cpp: High-level API usage example.
    @li aggregates_high_level.cpp: Shows how receive RESP3 aggregate data types in a general way or in STL containers.
    @li subscriber_high_level.cpp: Shows how channel [subscription](https://redis.io/topics/pubsub) works at a high-level.

    \b Asynchronous \b Servers (high-level API)

    @li echo_server.cpp: Asynchronous TCP server that echos user messages over Redis.
    @li chat_room.cpp: Shows how to build a scalable chat room.

    \section using-aedis Using Aedis

    To install and use Aedis you will need
  
    - Boost 1.78 or greater.
    - Unix Shell and Make.
    - C++14. Some examples require C++20 with coroutine support.
    - Redis server.

    Some examples will also require interaction with
  
    - redis-cli: Used in one example.
    - Redis Sentinel Server: used in some examples.

    Aedis has been tested with the following compilers

    - Tested with gcc: 7.5.0, 8.4.0, 9.3.0, 10.3.0.
    - Tested with clang: 11.0.0, 10.0.0, 9.0.1, 8.0.1, 7.0.1.
  
    \subsection Installation

    The first thing to do is to download and unpack Aedis

    ```
    # Download the latest release on github
    $ wget https://github.com/mzimbres/aedis/releases
  
    # Uncompress the tarball and cd into the dir
    $ tar -xzvf aedis-version.tar.gz
    ```

    If you can't use \c configure and \c make (e.g. Windows users)
    you can already add the directory where you unpacked Aedis to the
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

    \subsection Developers
  
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

    @li RESP3. Without RESP3 is impossible to support some important
    Redis features like client side caching, among other things.
    @li The Asio asynchronous model.
    @li Serialization of user data types that avoids temporaries.
    @li Error handling with error-code and exception overloads.
    @li Healthy checks.
    @li Fine control over memory allocation by means of allocators.

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
    @li Having to manually finish the pipeline with \c .exec() is a major source of headache. This is not required by the protocol itself but results from the abstraction used.
    @li Any Api that sends individual commands has a very restricted scope of usability and should be avoided in anything that needs minimum performance guarantees.
    @li The API imposes exceptions on users, no error-code overload is provided.
    @li No control over dynamic allocations.
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
    from the IO objects, for example

    @code
    std::string request;
    auto sr = make_serializer(request);
    sr.push(command::hello, 3);
    sr.push(command::multi);
    sr.push(command::ping, "Some message.");
    sr.push(command::set, "low-level-key", "some content", "EX", "2");
    sr.push(command::exec);
    sr.push(command::ping, "Another message.");
    net::write(socket, net::buffer(request));
    @endcode

    The request created above will be sent to Redis in a single
    pipeline and imposes no restriction on what it contains e.g. the
    number of commands, transactions etc. The problems mentioned above
    simply do not exist in Aedis. The way responses are read is
    also more flexible

    @code
    std::string buffer;
    auto dbuffer = net::dynamic_buffer(buffer);

    std::tuple<std::string, boost::optional<std::string>> response;
    resp3::read(socket, dbuffer); // hellp
    resp3::read(socket, dbuffer); // multi
    resp3::read(socket, dbuffer); // ping
    resp3::read(socket, dbuffer); // set
    resp3::read(socket, dbuffer, adapt(response));
    resp3::read(socket, dbuffer); // quit
    @endcode

    @li The response objects are passed by the caller to the read
    functions so that he has fine control over memory allocations and
    object lifetime.
    @li The user can either use error-code or exceptions.
    @li Each response can be read individually in the response object
    avoiding temporaries.
    @li It is possible to ignore responses.

    This was the blocking API, now let us compare the async interface

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
    enqueueing a message and triggering a write. It is also not clear
    how are pipelines realised with the design (if at all).

    In Aedis the send function looks like this

    @code
    template <class... Ts>
    void client::send(Command cmd, Ts const&... args);
    @endcode

    and the response is delivered through a callback.

    \section Acknowledgement

    Some people that were helpful in the development of Aedis

    @li Richard Hodges ([madmongo1](https://github.com/madmongo1)): For answering pretty much every question I had about Asio and the design of asynchronous programs.
    @li Vinícius dos Santos Oliveira ([vinipsmaker](https://github.com/vinipsmaker)): For useful discussion about how Aedis consumes buffers in the read operation (among other things).

    \section Reference
  
    See \subpage any.

 */

/** \defgroup any Reference
 *
 *  This page contains the documentation of all user facing code.
 */

#endif // AEDIS_HPP
