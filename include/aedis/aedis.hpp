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
    \tableofcontents
  
    \section documentation Documentation

    - \subpage reference
    - \subpage examples

    \section overview Overview

    Aedis is a redis client designed for scalability and performance
    while providing an easy and intuitive interface. Some of the features
    are

    - First class support to STL containers and C++ built-in types.
    - Support for pipelining, trasactions and TLS.
    - Serialization and deserializaiton of your own types.
    - First class async support with ASIO.
 */

//---------------------------------------------------------
// Pages

/** \page examples Examples
 *
    \b Basics: Focuses on small code snipets that show basic usage of
    the library.

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

    - response_queue.cpp

      In most cases commands will be added dynamically to the request
      for example as users interact with app. In order to process the
      responses asynchronously users have to keep a queue of the
      expected commands or use the one provided by the serializer
      class. This example shows how to do it.

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

    \b Customization \b points

    - serialization.cpp

      Shows how to de/serialize your own non-aggregate data-structures.

    - response_adapter.cpp

      Customization point for users that want to de/serialize their
      own data-structures.

    \b Adavanced: The main difference the examples below and the
    others above is that they user long lasting connections to Redis.
    This is the desired way to communicate with redis.

    - echo_server.cpp

      Shows the basic principles behind async communication with a
      database in a tcp server. In this case, the server is a proxy
      between the user and the database.
 */

/** \page reference Reference
 *
 *  Aedis source code reference.
  
    - \subpage enums
    - \subpage classes
    - \subpage operators
    - \subpage read_write_ops
    - \subpage functions
 */

/** \file aedis.hpp
 *  \brief Includes all headers that are necessary in order to use aedis.
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
