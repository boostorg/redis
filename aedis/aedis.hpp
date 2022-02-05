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
#include <aedis/redis/experimental/client.hpp>

/** \mainpage Documentation
    \tableofcontents
  
    \section Overview
  
    Aedis is low-level redis client library built on top of Boost.Asio
    that implements communication with a Redis server over its native
    protocol RESP3. It has first-class support for STL containers and
    C++ built in types among other things. You will be able to
    implement your own redis client or use a general purpose provided
    by the library. For more information about Redis see
    https://redis.io/
  
    \section examples Examples

    In general every feature offered by the library will be
    accompained by an example showing how to use it. We also focus in
    a more modern asynchronous programming with coroutines.

    \subsection Basics

    Focuses on small examples that show basic usage of
    the library, for example, how to make a request and read the
    response, how to deal with keys that may not exist, pubsub, etc.

    - intro.cpp: A good starting point. Some commands are sent to the
      Redis server and the responses are printed to screen. 

    - transaction.cpp: Shows how to read the responses to a trasaction
      efficiently. See also https://redis.io/topics/transactions.

    - multipurpose_response.cpp: Shows how to read any responses to
      Redis commands, including nested aggegagtes.

    - subscriber.cpp: Shows how channel subscription works at a low
      level. See also https://redis.io/topics/pubsub.

    - sync.cpp: Shows hot to use the Aedis synchronous api.

    - key_expiration.cpp: Shows how to use \c std::optional to deal
      with keys that may have expired or do not exist.

    \subsection stl-containers STL Containers

    Many of the Redis data structures can be
    directly translated in to STL containers, below you will find some
    example code. For a list of Redis data types see
    https://redis.io/topics/data-types.

    - hashes.cpp: Shows how to read Redis hashes in a \c std::map, \c
      std::unordered_map and \c std::vector.

    - lists.cpp: Shows how to read Redis lists in \c std::list,
      \c std::deque, \c std::vector. It also illustrates basic serialization.

    - sets.cpp: Shows how to read Redis sets in a \c std::set, \c
      std::unordered_set and \c std::vector.

    \subsection customization-points Customization points

    Shows how de/serialize user types
    avoiding copies. This is particularly useful for low latency
    applications that want to avoid unneeded copies, for examples when
    storing json strings in Redis keys.

    - serialization.cpp: Shows how to de/serialize your own
      non-aggregate data-structures.

    - response_adapter.cpp: Customization point for users that want to
      de/serialize their own data-structures like containers for example.

    \subsection async-servers Asynchronous servers

    Contains some non-trivial examples
    servers that interact with users and Redis asynchronously over
    long lasting connections using a higher level API.

    - multipurpose_client.cpp: Shows how to use and experimental high
      level redis client that keeps a long lasting connections to a
      redis server. This is the starting point for the next examples.

    - echo_server.cpp: Shows the basic principles behind asynchronous
      communication with a database in an asynchronous server. In this
      case, the server is a proxy between the user and Redis.

    - chat_room.cpp: Shows how to build a scalable chat room that
      scales to millions of users.

    \section using-aedis Using Aedis

    This section contains instructions on how to install aedis on your machine among other things.

    \subsection Requirements
  
    Aedis installation requires
  
    - Boost 1.78 or greater
    - Unix Shell
    - Make
  
    To use Aedis and build its examples and tests you will need
  
    - C++20 with coroutine support.
    - Redis server.
  
    Some examples will also require interaction with
  
    - Redis-client: used in on example.
    - Redis Sentinel server: used in some examples.
  
    \subsection Installation
  
    Start by downloading and configuring the library
  
    ```
    # Download the libray on github.
    $ wget github-link
  
    # Uncompress the tarball and cd into the dir
    $ tar -xzvf aedis-1.0.0.tar.gz && cd aedis-1.0.0
  
    # Run configure with appropriate C++ flags and your boost installation, for example
    $ CXXFLAGS="-std=c++20 -fcoroutines -g -Wall -Wno-subobject-linkage"\
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
  
    \subsubsection Windows
  
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

