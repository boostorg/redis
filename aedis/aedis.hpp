/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/resp3/read.hpp>
#include <aedis/adapter/adapt.hpp>
#include <aedis/adapter/error.hpp>
#include <aedis/redis/command.hpp>
#include <aedis/sentinel/command.hpp>
#include <aedis/generic/error.hpp>
#include <aedis/generic/client.hpp>
#include <aedis/generic/serializer.hpp>

/** \mainpage Documentation
    \tableofcontents
  
    \section Overview
  
    Aedis is a [Redis](https://redis.io/) client library built on top
    of [Asio](https://www.boost.org/doc/libs/release/doc/html/boost_asio.html)
    that provides simple and efficient communication with a Redis
    server. Some of its distinctive features are

    @li Support for the latest version of the Redis communication protocol [RESP3](https://github.com/redis/redis-specifications/blob/master/protocol/RESP3.md).
    @li First class support for STL containers and C++ built-in types.
    @li Serialization and deserialization of your own data types that avoid unnecessary copies.
    @li Support for Redis [sentinel](https://redis.io/docs/manual/sentinel).
    @li Sync and async API.

    In addition to that, Aedis provides a high level client that offers the following functionality

    @li Management of message queues.
    @li Simplified handling of server pushes.
    @li Zero asymptotic allocations by means of memory reuse.

    If you never heard about Redis the best place to start is on
    https://redis.io.  Now let us have a look at the low-level API.

    \section low-level-api Low-level API

    The low-level API is very useful for tasks that can be performed
    in short lived connections, for example, assume we want to perform
    the following steps

    @li Set the value of a Redis key.
    @li Set the expiration of that key to two seconds.
    @li Get and return its old value.
    @li Quit

    The coroutine-based async implementation of the steps above look like

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

    As stated above, request are created by defining a storage object
    and a serializer that knows how to convert user data into valid
    RESP3 wire-format.  Redis request are composed of one or more
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
    set      | Simple string, null or blob string  | https://redis.io/commands/set
    get      | Blob string                         | https://redis.io/commands/get
    smembers | Set                                 | https://redis.io/commands/smembers
    hgetall  | Map                                 | https://redis.io/commands/hgetall

    Once the RESP3 type of a given response is known we can choose a
    proper C++ data structure to receive it in. Fortunately, this is a
    simple task for most types. The table below summarise the options

    RESP3 type     | C++                                                          | Type
    ---------------|--------------------------------------------------------------|------------------
    Simple string  | \c std::string                                             | Simple
    Simple error   | \c std::string                                             | Simple
    Blob string    | \c std::string, \c std::vector                             | Simple
    Blob error     | \c std::string, \c std::vector                             | Simple
    Number         | `long long`, `int`, `std::size_t`, \c std::string          | Simple
    Double         | `double`, \c std::string                                   | Simple
    Null           | `boost::optional<T>`                                       | Simple
    Array          | \c std::vector, \c std::list, \c std::array, \c std::deque | Aggregate
    Map            | \c std::vector, \c std::map, \c std::unordered_map         | Aggregate
    Set            | \c std::vector, \c std::set, \c std::unordered_set         | Aggregate
    Push           | \c std::vector, \c std::map, \c std::unordered_map         | Aggregate

    Responses that contain nested aggregates or heterogeneous data
    types will be given special treatment laster.  As of this writing,
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

    In other words, it is pretty straightforward, just pass the result
    of \c adapt to the read function and make sure the response data
    type fits in the type you are calling @c adapter(...) with. All
    standard C++ containers are supported by aedis.

    \subsubsection Optional

    It is not uncommon for apps to access keys that do not exist or
    that have already expired in the Redis server, to deal with these
    cases Aedis provides support for \c boost::optional. To use it,
    wrap your type around \c boost::optional like this

    @code
    boost::optional<std::unordered_map<T, U>> umap;
    co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt(umap));
    @endcode

    Everything else stays pretty much the same, before accessing data,
    users will have to check or assert the optional contains a
    value.

    \subsubsection heterogeneous_aggregates Heterogeneous aggregates

    There are cases where Redis returns aggregates that
    contain heterogeneous data, for example, an array that contains
    integers, strings nested sets etc. Aedis supports reading such
    aggregates in a \c std::tuple efficiently as long as the they
    don't contain 2-order nested aggregates e.g. an array that
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

    Note that we are not ignoring the response to the commands
    themselves above but whether they have been successfully queued.
    Only after @c exec is received Redis will execute them in
    sequence. The response will then be sent in a single chunk to the
    client.

    \subsubsection Serialization

    As mentioned in \ref requests-serialization, it is common for
    users to serialized data before sending it to Redis e.g.  json
    strings, for example

    @code
    sr.push(command::set, "key", "{"Server": "Redis"}"); // Unquoted string
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

    As already mentioned, there are cases where the response to Redis
    commands won't fit in the model presented above, some examples are

    @li Commands (like \c set) whose response don't have a fixed
    RESP3 type. Expecting an \c int and receiving a blob string
    will result in error.
    @li RESP3 responses that contain three levels of (nested) aggregates can't be
    read in STL containers.
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

    As stated earlier, the low-level API is very useful for short
    lived connections, however, as long as we start using some Redis
    features the need for lived connections become apparent

    @li \b Server \b pushes: Short lived connections can't handle server pushes (e.g. https://redis.io/topics/client-side-caching and https://redis.io/topics/notifications).
    @li \b Pubsub: Just like server pushes, to use Redis pubsub users need long lasting connections (https://redis.io/topics/pubsub).
    @li \b Performance: Keep opening and closing connections impact performance.
    @li \b Pipeline: Code such as shown in \ref low-level-api don't support pipelines well since it can only send a fixed number of commands at time. It misses important optimization opportunities (https://redis.io/topics/pipelining).

    A serious implementation the supports the points listed above is
    far from trivial as it involves the following async operations

    @li \c async_resolve: Resolve a hostname.
    @li \c async_connect: Connect to Redis.
    @li \c async_read: Performed in a loop as long as the connection lives.
    @li \c async_write: Performed everytime a new message is added.

    In addition to that

    @li each operation listed above requires a timer that runs concurrently with it.
    @li \c async_write operations require management of the message queue to prevent concurrent writes.

    To avoid imposing this burden on every user, Aedis provides its
    own implementation. The general form of a program that uses the
    high-level api looks like this

    @code
    int main()
    {
       net::io_context ioc;

       client<net::ip::tcp::socket> db{ioc.get_executor()};

       db.set_resp3_handler(resp3_callback);
       db.set_read_handler(read_callback);

       db.async_run({net::ip::make_address("127.0.0.1"), 6379}, [](auto ec){ ... });

       ioc.run();
    }
    @endcode

    The \c async_run function above follows Asio's asynchrony model.
    For example, it is easy to implement a reconnect strategy when
    using a coroutine

    @code
    awaitable
    @endcode

    \subsection high-level-sending-cmds Sending commands

    The db object from the example above can be passed around to other
    objects so commands can be sent from everywhere in the app.
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
    queue and send them only if there is no pending response of a
    previously sent command. This is so because RESP3 is a
    request/response protocol, which means clients must wait for the
    response to a command before proceeding with the next one.

    \subsection high-level-responses Responses

    Aedis also provides some facilities to use use custom responses with the
    high-level API. Assume for example you have many different custom
    response types \c T1, \c T2 etc, a receiver that makes use of this looks
    like

    @code
    using responses_tuple_type = std::tuple<T1, T2, T3>;
    using adapters_tuple_type = adapters_t<responses_tuple_type>;

    class myreceiver {
    public:
       myreceiver(...) : adapters_(make_adapters_tuple(resps_)) , {}

       void on_connect()
       {
          db_->send(command::hello, 3);
       }

       void
       on_resp3(command cmd, node<boost::string_view> const& nd, boost::system::error_code& ec)
       {
          // Direct the responses to the desired adapter.
          switch (cmd) {
             case cmd1: adapter::get<T1>(adapters_)(nd, ec);
             case cmd2: adapter::get<T2>(adapters_)(nd, ec);
             case cmd3: adapter::get<T2>(adapters_)(nd, ec);
             default:;
          }
       }

       void on_read(command cmd, std::size_t)
       {
          switch (cmd) {
             case cmd1: // Data on std::get<T1>(resps_); break;
             case cmd2: // Data on std::get<T2>(resps_); break;
             case cmd3: // Data on std::get<T3>(resps_); break;
             default:;
          }
       }

       void on_write(std::size_t n) { ... }

       void on_push() { ... }

    private:
       responses_tuple_type resps_;
       adapters_tuple_type adapters_;
    };
    @endcode

    \section examples Examples

    To better fix what has been said above, users should have a look at some simple examples.

    \b Low \b level \b API

    @li low_level/sync_intro.cpp: Shows how to use the Aedis synchronous api.
    @li low_level/sync_serialization.cpp: Shows how serialize your own type avoiding copies.
    @li low_level/async_intro.cpp: Show how to use the low level async api.
    @li low_level/subscriber.cpp: Shows how channel subscription works at the low level.
    @li low_level/adapter.cpp: Shows how to write a response adapter that prints to the screen, see \ref low-level-adapters.

    \b High \b level \b API

    @li high_level/intro.cpp: Some commands are sent to the Redis server and the responses are printed to screen. 
    @li high_level/aggregates.cpp: Shows how receive RESP3 aggregate data types in a general way.
    @li high_level/stl_containers.cpp: Shows how to read responses in STL containers.
    @li high_level/serialization.cpp: Shows how to de/serialize your own data types.
    @li high_level/subscriber.cpp: Shows how channel subscription works at a high level. See also https://redis.io/topics/pubsub.

    \b Asynchronous \b Servers

    @li high_level/echo_server.cpp: Shows the basic principles behind asynchronous communication with a database in an asynchronous server.
    @li high_level/chat_room.cpp: Shows how to build a scalable chat room that scales to millions of users.

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
    you can already add the directory where you unpacked aedis to the
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

    \section Acknowledgement

    I would like to thank Vinícius dos Santos Oliveira for useful discussion about how Aedis consumes buffers in read operation (among other things).

    \section Referece
  
    See \subpage any.
 */

/** \defgroup any Reference
 */

