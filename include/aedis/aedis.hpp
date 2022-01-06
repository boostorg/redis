/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/resp3/read.hpp>
#include <aedis/resp3/write.hpp>
#include <aedis/resp3/response_traits.hpp>
#include <aedis/resp3/serializer.hpp>
#include <aedis/resp3/client_base.hpp>


/** \mainpage
    \tableofcontents
  
    \section reference Reference

    - \subpage enums
    - \subpage classes
    - \subpage operators
    - \subpage read_write_ops
    - \subpage functions

    \section documentation Documentation

    - \subpage usage
    - \subpage tutorial

    \section overview Overview

    Aedis is a redis client designed for scalability and performance
    while providing an easy and intuitive interface. Some of the features
    are

    - First class support to STL containers and C++ built-in types.
    - Support for pipelining, trasactions and TLS.
    - Serialization and deserializaiton of your own types.
    - First class async support with ASIO.
 */

/** \page usage Usage
  
   Aedis is a header only library. You only need to include the header

   @code
   #include <aedis/src.hpp>
   @endcode

   in one of your source files.
 */

/** \page tutorial Tutorial
  
    Some info

    \section requests Requests

    Some text

    \section responses Responses

    Some text
 */

/** \file aedis.hpp
 *  \brief Includes all headers that are necessary in order to use aedis.
 */


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
 *  \brief alkjd ajs
 */


/** \defgroup operators Operators
 *  \brief alkjd ajs
 */
