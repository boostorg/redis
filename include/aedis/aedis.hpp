/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/adapter/adapt.hpp>
#include <aedis/adapter/error.hpp>

#include <aedis/redis/command.hpp>
#include <aedis/redis/client.hpp>
#include <aedis/redis/receiver_base.hpp>

#include <aedis/sentinel/command.hpp>
#include <aedis/sentinel/client.hpp>
#include <aedis/sentinel/receiver_base.hpp>

/** \mainpage Documentation
    \tableofcontents
  
    \section Overview
  
    Aedis is a redis client library built on top of Asio that provides
    simple and efficient communication with a Redis server. Some of
    its distinctive features are

    @li Support for the latest version of the Redis communication protocol RESP3.
    @li First class support for STL containers and C++ built-in types.
    @li Serialization and deserialization of your own data types built directly into the parser to avoid temporaries.
    @li Sentinel support (https://redis.io/docs/manual/sentinel).
    @li Sync and async API.

    In addition to that, Aedis provides a high level client that offers the following functionality

    @li Management of message queues.
    @li Simplified handling of server pushs.
    @li Zero asymptotic allocations by means of memory reuse.

    If you never heard about Redis the best place to start is on
    https://redis.io.  Now let us have a look at the low-level API.

    \section low-level-api Low-level API

    The low-level API is very usefull for simple tasks, for example,
    assume we want to perform the following steps

    @li Set the value of a Redis key.
    @li Set the expiration of that key to two seconds.
    @li Get and return its old value.
    @li Quit

    The async coroutine-based implementation of the steps above look like

    @code
    net::awaitable<std::string> set(net::ip::tcp::endpoint ep)
    {
       // To make code less verbose
       using tcp_socket = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::socket>;

       tcp_socket socket{co_await net::this_coro::executor};
       co_await socket.async_connect(ep);
    
       std::string request, read_buffer, response;

       auto sr = make_serializer(request);
       sr.push(command::hello, 3);
       sr.push(command::set, "low-level-key", "Value", "EX", "2", "get");
       sr.push(command::quit);
       co_await net::async_write(socket, net::buffer(request));
    
       co_await resp3::async_read(socket, dynamic_buffer(read_buffer)); // Hello (ignored).
       co_await resp3::async_read(socket, dynamic_buffer(read_buffer), adapt(response)); // Set
       co_await resp3::async_read(socket, dynamic_buffer(read_buffer)); // Quit (ignored)
    
       co_return response;
    }
    @endcode

    The simplicity of the code above makes it self expanatory

    @li Connect to the Redis server.
    @li Declare a \c std::string to hold the request and add some commands in it with a serializer.
    @li Write the payload to the socket and read the responses in the same order they were sent.
    @li Return the response to the user.

    The @c hello command above is always required and must be sent
    first as it informs we want to communicate over RESP3.

    \subsection requests Requests

    As stated above, request are created by defining a storage object
    and a serializer that knowns how to convert user data into valid
    RESP3 wire-format.  Redis request are composed of one or more
    commands (in Redis documentation they are called pipelines, see
        https://redis.io/topics/pipelining), which means users can add
    as many commands to the request as they like, a feature that aids
    performance.

    The individual commands in a request assume many
    different forms: with and without keys, variable length arguments,
    ranges etc.  To account for all these variations, the \c
    serializer class offers some member functions, each of
    them with a couple of overloads, for example

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

    // As above but an iterator range.
    sr.push_range2(command::subscribe, std::cbegin(list), std::cend(list));

    // Sends a container, with key.
    sr.push_range(command::hset, "key", map);

    // As above but as iterator range.
    sr.push_range2(command::hset, "key", std::cbegin(map), std::cend(map));
    @endcode

    Once all commands we want to send to Redis have been added to the
    request, we can write it as usual to the socket.

    @code
    co_await net::async_write(socket, buffer(request));
    @endcode

    \subsubsection Serialization

    In general the \c send and \c send_range functions above work with integers
    and \c std::string.  To send your own data type defined the \c to_bulk
    function like this

    @code
    // Example struct.
    struct mystruct {
      // ...
    };
    
    void to_bulk(std::string& to, mystruct const& obj)
    {
       // Convert to obj string and call
       aedis::resp3::to_bulk(to, "Dummy serializaiton string.");
    }

    std::map<std::string, mystruct> map
       { {"key1", {...}}
       , {"key2", {...}}
       , {"key3", {...}}};

    db.send_range(command::hset, "key", map);
    @endcode

    It is quite common to store json string in Redis for example.

    \subsection responses Responses

    To read the responses to comands effectively, users must know the
    RESP3 type of the response, this can be found in the Redis
    documentation of each command (https://redis.io/commands). For example

    Command  | RESP3 type                          | Documentation
    ---------|-------------------------------------|--------------
    lpush    | Number                              | https://redis.io/commands/lpush
    lrange   | Array                               | https://redis.io/commands/lrange
    set      | Simple string, null or blob string  | https://redis.io/commands/set
    get      | Blob string                         | https://redis.io/commands/get
    smembers | Set                                 | https://redis.io/commands/smembers
    hgetall  | Map                                 | https://redis.io/commands/hgetall

    Once the RESP3 type of a given response is known we can choose a
    proper C++ data structure to receive it in. Fourtunately,
    this is a simple task for most types, for example

    RESP3 type     | C++                                                          | Type
    ---------------|--------------------------------------------------------------|------------------
    Simple string  | \c std::string                                             | Simple
    Simple error   | \c std::string                                             | Simple
    Blob string    | \c std::string, \c std::vector                             | Simple
    Blob error     | \c std::string, \c std::vector                             | Simple
    Number         | `long long`, `int`, \c std::string                         | Simple
    Null           | `boost::optional<T>`                                       | Simple
    Array          | \c std::vector, \c std::list, \c std::array, \c std::deque | Aggregate
    Map            | \c std::vector, \c std::map, \c std::unordered_map         | Aggregate
    Set            | \c std::vector, \c std::set, \c std::unordered_set         | Aggregate
    Push           | \c std::vector, \c std::map, \c std::unordered_map         | Aggregate

    Exceptions to this rule are responses that contain nested
    aggregates or heterogeneuos data types, those will be treated
    later.  As of this writing, not all RESP3 types are used by the
    Redis server, which means in practice users will be concerned with
    a reduced subset of the RESP3 specification. Now let us see some
    examples

    @code
    // To ignore the response.
    co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt());

    // To read in a std::string e.g. get.
    std::string str;
    co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt(str));

    // To read in a long long e.g. rpush.
    long long number;
    co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt(number));

    // To read in a std::set e.g. smembers.
    std::set<T, U> set;
    co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt(set));

    // To read in a std::map e.g. hgetall.
    std::map<T, U> set;
    co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt(map));

    // To read in a std::unordered_map e.g. hgetall.
    std::unordered_map<T, U> umap;
    co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt(umap));

    // To read in a std::vector e.g. lrange.
    std::vector<T, U> vec;
    co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt(vec));
    @endcode

    In other words, it is pretty straightforward, just pass the result
    of \c adapt to the read function and make sure the response RESP3
    type fits in the type you are calling @c adapter(...) with. All
    C++ containers are supported by aedis.

    \subsubsection Optional

    It is not uncommon for apps to access keys that do not exist or
    that have already expired in the Redis server, to support such usecases Aedis
    provides support for \c boost::optional. To use it just wrap your
    type around \c boost::optional like this

    @code
    boost::optional<std::unordered_map<T, U>> umap;
    co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt(umap));
    @endcode

    Everything else stays pretty much the same, before accessing data
    users will have to check (or assert) the optional contains a
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

    Note that above we are not ignoring the response to the comands
    themselves but whether they have been successfully queued.  Only
    after @c exec is received Redis will execute them. The response
    will then be sent in a single chunck to the client.

    \subsubsection Serialization

    It is rather common for users to store serialized data in Redis
    such as json strings, for example

    @code
    sr.push(command::set, "key", "json-string")
    sr.push(command::get, "key")
    @endcode

    For performance and convenience reasons, some users will want to
    avoid receiving the response to the \c get command above in a string
    to later convert it into a e.g. deserialized json. To support this,
    Aedis calls a user defined \c from_string function while parsing
    the response. In simple terms, define your type

    @code
    struct mystruct {
       // struct fields.
    };
    @endcode

    and deserialize it from a string in a function \c from_string

    @code
    void from_string(mystruct& obj, char const* p, std::size_t size, boost::system::error_code& ec)
    {
       // Deserializes p into obj.
    }
    @endcode

    After that, we can start receiving data efficiently in the desired
    types e.g. \c mystruct, \c std::map<std::string, mystruct> etc.

    \subsubsection gen-case The general case

    As already mentioned, there are cases where the response to Redis
    commands won't fit in the model presented above, some examples are

    @li Commands (like \c set) whose response don't have a fixed
    RESP3 type. Expecting an e.g. \c int and receiving a e.g. blob string
    will result in error.
    @li RESP3 aggregates that contain nested aggregates can't be
    read in STL containers e.g. command, etc..
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

    For example, suppose we want to retrieve the a hash data structure
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
    auto adapter = [](resp3::type t, std::size_t aggregate_size, std::size_t depth, char const* value, std::size_t size, boost::system::error_code&)
    {
       std::cout
          << "type: " << t << "\n"
          << "aggregate_size: " << aggregate_size << "\n"
          << "depth: " << depth << "\n"
          << "value: " << std::string_view{value, size} << "\n";
    };

    co_await resp3::async_read(socket, dynamic_buffer(buffer), adapter);
    @endcode

    See more in the \ref examples section.

    \section high-level-api High-level API

    It requires a lot of further work to make use of many important features
    of the Redis server when using the low-level API, for example

    @li \b Server \b pushes: Short lived connections can't handle server pushes (e.g. https://redis.io/topics/client-side-caching and https://redis.io/topics/notifications).
    @li \b Pubsub: Just like server pushes, to use Redis pubsub support users need long lasting connections (https://redis.io/topics/pubsub).
    @li \b Performance: Keep opening and closing connections impact performance.
    @li \b Pipeline: Code such as shown in \ref low-level-api don't support pipelines well since it can only send a fixed number of commands. It misses important optimization oportunities (https://redis.io/topics/pipelining).

    To avoid these drawbacks users will address the points above
    reinventing the high-level API here and there over and over again,
    to prevent that from happening Aedis provides its own. The general
    form of a program that uses the high-level api looks like this

    @code
    int main()
    {
       net::io_context ioc;
       client<net::ip::tcp::socket> db{ioc.get_executor()};
       receiver recv;

       db.async_run(
           recv,
           {net::ip::make_address("127.0.0.1"), 6379},
           [](auto ec){ ... });

       // Pass db around to other objects so we can send commands.

       ioc.run();
    }
    @endcode

    The only thing users have to care about is with the implementation
    of the \c receiver class, everything else will be done
    automatically by the client class.  To simplify it even further
    users can (but don't have to) use a base class that abstracts most
    of the complexity away. The general form of a receiver looks like
    this

    @code
    class myreceiver : public receiver_base<T1, T2, T3, ...> {
    public:
       int to_index_impl(command cmd) override
       {
          // Directs responses to the desired types.
          switch (cmd) {
             case cmd1: return index_of<T1>();
             case cmd2: return index_of<T2>();
             case cmd3: return index_of<T3>();
             ...
             default: return -1; // Ignore a command.
          }
       }

       void on_read_impl(command cmd) override
       {
         // Called when the response is received. Response available with get<T1>().
       }

       void on_push_impl() override
       {
         // Called when a server push is received. Response available with get<T1>().
       }
    };
    @endcode

    Where \c T1, \c T2 etc. are any of the response types discussed in \ref low-level-api. Sending
    commands is also similar to what has been discussed there.

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

    \subsection high-level-transaction Transactions

    Aedis won't pass the responses of \c multi or any other
    commands inside the transaction to the user, only the final result
    i.e.  the response to \c exec. The reason
    for this behaviour is that unless there is a programming error,
    the response to the individual commands that precede \c exec
    can't fail, they will always be "QUEUED", or as quoted form the
    Redis documentation https://redis.io/topics/transactions:

    > Redis commands can fail only if called with a wrong syntax (and
    > the problem is not detectable during the command queueing),
    > or against keys holding the wrong data type: this means that in
    > practical terms a failing command is the result of a programming
    > errors, and a kind of error that is very likely to be detected
    > during development, and not in production.

    \subsection high-level-receiver Receiver

    Just as users can implement their own adapter for the low-level API,
    they can also implement their own Receiver for the high-level API.
    For example

    @code
    struct receiver {
       void on_resp3(command cmd, type t, std::size_t aggregate_size, std::size_t depth, char const* data, std::size_t size, boost::system::error_code&)
       {
          std::cout << "Resp3 data received" << std::endl;
       }

       void on_push()
       {
          std::cout << "on_push: " << std::endl;
       }

       void on_read(command cmd)
       {
          std::cout << "on_read: " << cmd << std::endl;
       }

       void on_write(std::size_t n)
       {
          std::cout << "on_write: " << n << std::endl;
       }
    };
    @endcode

    \section examples Examples

    To better fix what has been said above, users should have a look at some simple examples.

    \b Low \b level \b API

    @li low_level/sync_intro.cpp: Shows how to use the Aedis synchronous api.
    @li low_level/async_intro.cpp: Show how to use the low level async api.
    @li low_level/subscriber.cpp: Shows how channel subscription works at a low level.
    @li low_level/adapter.cpp: Shows how to write a response adapter that prints to the screen, see \ref low-level-adapters.

    \b High \b level \b API

    @li high_level/intro.cpp: A good starting point. Some commands are sent to the Redis server and the responses are printed to screen. 
    @li high_level/aggregates.cpp: Shows how receive RESP3 aggregate data types in a general way.
    @li high_level/stl_containers.cpp: Shows how to read responses in STL containers.
    @li high_level/serialization.cpp: Shows how to de/serialize your own non-aggregate data-structures.
    @li high_level/subscriber.cpp: Shows how channel subscription works at a high level. See also https://redis.io/topics/pubsub.
    @li high_level/echo_server.cpp: Shows the basic principles behind asynchronous communication with a database in an asynchronous server. In this case, the server is a proxy between the user and Redis.
    @li high_level/chat_room.cpp: Shows how to build a scalable chat room that scales to millions of users.
    @li high_level/receiver.cpp: Customization point for users that want to de/serialize their own data-structures like containers for example.

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

    If don't want to use \c configure and \c make (e.g. Windows users)
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
    compiler other that the system compiler use
  
    ```
    CC=/opt/gcc-10.2.0/bin/gcc-10.2.0 CXX=/opt/gcc-10.2.0/bin/g++-10.2.0 CXXFLAGS="-fcoroutines -g -Wall -Werror"  ./configure ...
    ```

    \section Referece
  
    See \subpage any.
 */

/** \defgroup any Reference
 */

