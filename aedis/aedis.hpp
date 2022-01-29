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

/** \mainpage
 *
 *  \b Overview
 *
 *  Aedis is low-level redis client built on top of Boost.Asio that
 *  implements communication with a Redis server over its native
 *  protocol RESP3. It has first-class support for STL containers and
 *  C++ built in types. You will be able to implement your own redis
 *  client or use a general purpose provided by the library. For more
 *  information about Redis see https://redis.io/
 *
 *  \b Using \b Aedis
 *
 *  - \subpage installation
 *  - \subpage examples
 *
 *  \b Reference
 *
 *  - \subpage enums
 *  - \subpage classes
 *  - \subpage functions
 *  - \subpage operators
 */

//---------------------------------------------------------
// Pages

/** \page examples Examples
 *
    \b Basics: Focuses on small code snippets that show basic usage of
    the library, for example: how to make a request and read the
    response, how to deal with keys that may not exist, etc.

    - intro.cpp

      Some commands are sent to the Redis server and the responses are
      printed to screen.

    - key_expiration.cpp
      
      Shows how to use \c std::optional to deal with keys that may
      have expired or do not exist.

    - transaction.cpp

      Shows how to read the responses to all commands inside a
      trasaction efficiently. At the moment this feature supports only
      transactions that contain simple types or aggregates that don't
      contain aggregates themselves (as in most cases).

    - nested_response.cpp
      
      Shows how to read responses to commands that cannot be
      translated in a C++ built-in type like std::string or STL
      containers, for example all commands contained in a transaction
      will be nested by Redis in a single response.

    - subscriber.cpp

      Shows how channel subscription works at a low level.

    - sync.cpp
      
      Shows hot to use the Aedis synchronous api.

    \b STL \b Containers: Many of the Redis data structures can be
    directly translated in to STL containers, below you will find some
    example code. For a list of Redis data types see
    https://redis.io/topics/data-types.

    - hashes.cpp

      Shows how to read Redis hashes in a \c std::map, \c
      std::unordered_map and \c std::vector.

    - lists.cpp

      Shows how to read Redis lists in \c std::list,
      \c std::deque, \c std::vector. It also illustrates basic serialization.

    - sets.cpp

      Shows how to read Redis sets in a \c std::set, \c std::unordered_set
      and \c std::vector.

    \b Customization \b points: Shows how de/serialize user types
    avoiding copies. This is particularly useful for low latency
    applications.

    - serialization.cpp

      Shows how to de/serialize your own non-aggregate data-structures.

    - response_adapter.cpp

      Customization point for users that want to de/serialize their
      own data-structures.

    \b Asynchronous \b servers: Contains some non-trivial examples
    servers that interact with users and Redis asynchronously over
    long lasting connections using a higher level API.

    - redis_client.cpp

      Shows how to use and experimental high level redis client that
      keeps a long lasting connections to a redis server. This is the
      starting point for the next examples.

    - echo_server.cpp

      Shows the basic principles behind asynchronous communication
      with a database in an asynchronous server. In this case, the
      server is a proxy between the user and the database.

    - chat_room.cpp

      Shows how to build a scalable chat room that scales to
      millions of users.
 */

/** \page installation Installation
 *
 *  \section Requirements
 *
 *  To use Aedis you will need
 *
 *  Required
 *
 *  - \b C++20 with \b coroutine support.
 *  - \b Boost \b 1.78 or greater.
 *  - \b Redis \b server.
 *  - \b Bash to configure that package for installation.
 *
 *  Optional
 *
 *  - \b Redis \b Sentinel \b server: used in some examples..
 *  - \b Redis \b client: used in some examples.
 *  - \b Make to build the examples and tests.
 *
 *  \section Installing
 *
 *  Download
 *
 *  Get the latest release and follow the steps
 *
 *  ```bash
 *  # Download the libray on github.
 *  $ wget github-link
 *
 *  # Uncompress the tarball and cd into the dir
 *  $ tar -xzvf aedis-1.0.0.tar.gz && cd aedis-1.0.0
 *
 *  # Run configure with appropriate C++ flags and your boost installation, for example
 *  $ CXXFLAGS="-std=c++20 -fcoroutines -g -Wall -Wno-subobject-linkage"\
 *  ./configure  --prefix=/opt/aedis-1.0.0 --with-boost=/opt/boost_1_78_0 --with-boost-libdir=/opt/boost_1_78_0/lib
 *
 *  ```
 *
 *  Install
 *
 *  ```bash
 *  # Optional: Build aedis examples.
 *  $ make examples
 *
 *  # Optional: Test aedis in your machine.
 *  $ make check
 *
 *  # Install the aedis.
 *  $ sudo make install
 *  ```
 *
 *  \section using Using Aedis
 *
 *  This library in not header-only. You have to include the following
 *  header 
 *
 *  ```cpp
 *  #include <aedis/src.hpp>
 *  ```
 *
 *  in exactly one source file in your applications.
 *
 *  \section Developers
 *
 *  Aedis uses Autotools for its build system. To generate the build
 *  system run
 *
 *  ```bash
 *  $ autoreconf -i
 *  ```
 *
 *  After that you will have a config in the project dir that you can
 *  run as explained above, for example, to use a compiler other that
 *  the system compiler use
 *
 *  ```bash
 *  CC=/opt/gcc-10.2.0/bin/gcc-10.2.0\
 *  CXX=/opt/gcc-10.2.0/bin/g++-10.2.0\
 *  CXXFLAGS="-std=c++20 -fcoroutines -g -Wall -Wno-subobject-linkage -Werror"\
 *  ./configure ...
 *  ```
 */

//---------------------------------------------------------
// Groups

/** \defgroup enums Enumerations
 *  \brief Enumerations defined by this library.
 */


/** \defgroup classes Classes
 *  \brief Classes defined by this library.
 */


/** \defgroup functions Free functions
 *  \brief All functions defined by this library.
 */


/** \defgroup operators Operators
 *  \brief Operators defined in Aedis
 */
