/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/resp3/read.hpp>
#include <aedis/resp3/write.hpp>
#include <aedis/resp3/serializer.hpp>

/** \mainpage
  
    Introduction

    - \subpage overview
    - \subpage installation
    - \subpage examples

    Reference

    - \subpage enums
    - \subpage classes
    - \subpage read_write_ops
    - \subpage functions
    - \subpage operators
 */

//---------------------------------------------------------
// Pages

/** \page overview Overview
 *
 *  Aedis provides low-level communication with a Redis server over
 *  its native protocol RESP3. Some of its featues are
 *
 *  - First class support to STL containers and C++ built-in types.
 *  - Support for pipelining, trasactions and TLS.
 *  - Serialization and deserializaiton of your own types.
 *  - First class support to async programming through ASIO.
 */

/** \page examples Examples
 *
    \b Basics: Focuses on small code snipets that show basic usage of
    the library. For example: How to create a request and read resp,
    how to deal with keys that may not exist, etc.

    - intro.cpp

      Users should start here. Some commands are sent to the Redis
      server and the responses are printed to screen.

    - key_expiration.cpp
      
      Shows how to use \c std::optional to deal with keys that may
      have expired or do not exist.

    - nested_response.cpp
      
      Shows how to read responses to commands that cannot be
      translated in a C++ built-in type like std::string or STL
      containers.

    - subscriber.cpp

      Shows how channel subscription works.

    - sync.cpp
      
      Aedis also offers a synchronous api, this example shows how to use it.

    \b STL \b Containers: Many of the Redis data structures can be
    directly translated in to STL containers. The examples bellow show
    how to do that. The list of Redis data types can be found at
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

    \b Customization \b points: Shows how de/serialize user types avoiding copies. This is specially useful for low latency applicaitons.

    - serialization.cpp

      Shows how to de/serialize your own non-aggregate data-structures.

    - response_adapter.cpp

      Customization point for users that want to de/serialize their
      own data-structures.

    \b Adavanced: Contains some non-trivial examples that user
    long-lasting TCP connections to the Redis server. This is the
    desired way to communicate with redis. Some of the source code is
    contained in \c lib sub-directory.

    - echo_server.cpp

      Shows the basic principles behind async communication with a
      database in an asynchronous server. In this case, the server is
      a proxy between the user and the database.

    - chat_room.cpp

      Shows how to build a scallable chat room where you can scale to
      millions of users.
 */

/** \page installation Intallation
 *
 *  This library is header only. To install it run
 *
 *  ```cpp
 *  $ sudo make install
 *  ```
 *
 *  or copy the include folder to the location you want.  You will
 *  also need to include the following header in one of your source
 *  files e.g. `aedis.cpp`
 *
 *  ```cpp
 *  #include <aedis/impl/src.hpp>
 *  ```
 */

//---------------------------------------------------------
// Groups

/** \defgroup enums Enums
 *  \brief Enums defined by this library.
 */


/** \defgroup classes Classes
 *  \brief Classes defined by this library.
 */


/** \defgroup functions Free functions (other)
 *  \brief All functions defined by this library.
 */


/** \defgroup read_write_ops Free functions (read/write operations)
 *  \brief RESP3 read and write functions.
 */


/** \defgroup operators Operators
 *  \brief Operators defined in Aedis
 */
