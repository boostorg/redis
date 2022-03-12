/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/config.hpp>
#include <aedis/redis/command.hpp>
#include <aedis/sentinel/command.hpp>
#include <aedis/resp3/type.hpp>
#include <aedis/resp3/read.hpp>
#include <aedis/resp3/adapt.hpp>
#include <aedis/resp3/error.hpp>
#include <aedis/resp3/serializer.hpp>
#include <aedis/resp3/response_traits.hpp>
#include <aedis/redis/client.hpp>
#include <aedis/redis/receiver.hpp>

/** \mainpage Documentation
    \tableofcontents
  
    \section Overview
  
    Aedis is a low-level redis client library that provides simple and
    efficient communication with a Redis server (see https://redis.io/).
    It is built on top of Boost.Asio and some of its distinctive features are

    1. Support for the latest version of the Redis communication protocol RESP3.
    2. Asynchronous interface that handles servers pushs optimally.
    3. First class support for STL containers and C++ built-in types.
    4. Serialization and deserialization of your own data types built directly into the parser to avoid temporaries.
    5. Client class that abstracts the management of the output message queue away from the user.
    6. Asymptotic zero allocations by means of memory reuse.

    The general form of a program looks like this

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

       ioc.run();
    }
    @endcode

    Most of the time users will be concerned only with the
    implementation of the \c receiver class, to make that simpler,
    Aedis provides a base receiver class that abstracts most of the
    complexity away from the user. In a typical implementation the
    following functions will be overriden by the user

    @code
    class myreceiver : receiver<response_type> {
    public:
       void on_read_impl(command cmd) override
       {
         // ...
       }

       void on_write_impl(std::size_t n) override
       {
         // ...
       }

       void on_push_impl() override
       {
         // ...
       }
    };
    @endcode

    The \c client object (\c db above) can be passed
    around in the program to send commands to Redis, for example

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

    Now let us see how to make requests and receive responses with more detail.

    \subsection Requests

    Request are composed of one or more commands (in Redis
    documentation they are called pipeline, see
    https://redis.io/topics/pipelining). The individual commands in a
    request assume many different forms

    1. With key: \c set, \c get, \c expire etc.
    2. Without key: \c hello, \c quit etc.
    3. Ranges with a key: \c rpush, \c hgtall etc.
    4. Ranges without a key: \c subscribe,  etc.
    5. etc.

    To account for all these possibilities, the \c client class offer
    some member functions, for example

    @code
    // Some data to send to Redis.
    std::string value = "some value";

    std::list<std::string> list {"channel1", "channel2", "channel3"};

    std::map<std::string, mystruct> map
       { {"key1", "value1"}
       , {"key2", "value2"}
       , {"key3", "value3"}};

    // No key or arguments.
    db.>send(command::quit);

    // With key and arguments.
    db.>send(command::set, "key", value, "EX", "2");

    // Sends a container, no key
    db.>send_range(command::subscribe, list);

    // As above but an iterator range.
    db->send_range2(command::subscribe, std::cbegin(list), std::cend(list));

    // Sends a container, with key.
    db->send_range(command::hset, "key", map);

    // As above but as iterator range.
    db->send_range2(command::hset, "key", std::cbegin(map), std::cend(map));
    @endcode

    The \c send functions above adds commands to the output queue and
    send only if there is no pending response of a previously sent
    command. Users can also easily send their own data types to Redis
    by defining a to_string function, for example

    @code
    struct mystruct {
      int a;
      int b;
    };
    
    std::string to_string(mystruct const& obj)
    {
       // Convert to string
    }

    std::map<std::string, mystruct> map
       { {"key1", {1, 2}}
       , {"key2", {3, 4}}
       , {"key3", {5, 6}}};

    db.send_range(command::hset, "serialization-hset-key", map);
    @endcode

    See https://redis.io/commands/hset for \c hset documentation.

    \subsection Responses

    The Redis protocol RESP3 defines two types of data, they are.

    1. Simple data types: simple string, simple error, \c blob string, \c blob error, number, double, etc.
    2. Aggregates: array, push, set, map, etc.

    Most of these types can be translated into C++ containers and bilt-in types, for example

    1. \c std::string: simple and blob string and error.
    2. `long long int`: number
    3. \c double: double.

    https://redis.io/topics/data-types.

    \subsubsection Optional
    \subsubsection Transactions
    \subsubsection Serializaiton

    See also https://redis.io/topics/transactions.

    Redis commands can fail only if called with a wrong syntax (and
    the problem is not detectable during the command queueing),
    or against keys holding the wrong data type: this means that in
    practical terms a failing command is the result of a programming
    errors, and a kind of error that is very likely to be detected
    during development, and not in production.
  
    \section examples Examples

    - intro.cpp: A good starting point. Some commands are sent to the
      Redis server and the responses are printed to screen. 

    - aggregates.cpp: Shows how receive RESP3 aggregate data types.

    - stl_containers.cpp:

    - serialization.cpp: Shows how to de/serialize your own
      non-aggregate data-structures.
    - subscriber.cpp: Shows how channel subscription works at a low
      level. See also https://redis.io/topics/pubsub.

    - sync.cpp: Shows hot to use the Aedis synchronous api.

    - echo_server.cpp: Shows the basic principles behind asynchronous
      communication with a database in an asynchronous server. In this
      case, the server is a proxy between the user and Redis.

    - chat_room.cpp: Shows how to build a scalable chat room that
      scales to millions of users.

    - receiver.cpp: Customization point for users that want to
      de/serialize their own data-structures like containers for example.

    \section using-aedis Using Aedis

    To install and use Aedis you will need
  
    - Boost 1.78 or greater.
    - Unix Shell and Make.
    - Compiler with C++20 coroutine support e.g. GCC 10 or greater.
    - Redis server.
  
    Some examples will also require interaction with
  
    - redis-cli: used in one example.
    - Redis Sentinel Server: used in some examples.
  
    \subsection Installation
  
    Start by downloading and configuring the library
  
    ```
    # Download the latest release on github
    $ wget https://github.com/mzimbres/aedis/releases
  
    # Uncompress the tarball and cd into the dir
    $ tar -xzvf aedis-1.0.0.tar.gz && cd aedis-1.0.0
  
    # Run configure with appropriate C++ flags and your boost
    # installation, for example # You may also have to use
    # -Wno-subobject-linkage on gcc.
    $ CXXFLAGS="-std=c++20 -fcoroutines -g -Wall"\
    ./configure  --prefix=/opt/aedis-1.0.0 --with-boost=/opt/boost_1_78_0 --with-boost-libdir=/opt/boost_1_78_0/lib
  
    ```
  
    To install the library run
  
    ```
    # Install Aedis in the path specified in --prefix
    $ sudo make install
  
    ```
  
    At this point you can start using Aedis. To build the examples and
    test you can also run
  
    ```
    # Build aedis examples.
    $ make examples
  
    # Test aedis in your machine.
    $ make check
    ```
  
    Finally you will have to include the following header 
  
    ```cpp
    #include <aedis/src.hpp>
    ```
    in exactly one source file in your applications.
  
    Windows users can use aedis by either adding the project root
    directory to their include path or manually copying to another
    location. 
  
    \subsection Developers
  
    To generate the build system run
  
    ```
    $ autoreconf -i
    ```
  
    After that you will have a config in the project dir that you can
    run as explained above, for example, to use a compiler other that
    the system compiler use
  
    ```
    CC=/opt/gcc-10.2.0/bin/gcc-10.2.0\
    CXX=/opt/gcc-10.2.0/bin/g++-10.2.0\
    CXXFLAGS="-std=c++20 -fcoroutines -g -Wall -Wno-subobject-linkage -Werror"  ./configure ...
    ```
    \section Referece
  
    See \subpage any.
 */

/** \defgroup any Reference
 */

