/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/version.hpp>
#include <aedis/resp3/read.hpp>
#include <aedis/resp3/write.hpp>
#include <aedis/resp3/response.hpp>
#include <aedis/resp3/serializer.hpp>

/** \mainpage Aedis
  
   \section intro_sec Introduction
  
   Aedis is an async redis client built on top of Boost.Asio.

   \section usage_sec Usage

   Aedis is a header only library. You only need to include the header

   @code
   #include <aedis/src.hpp>
   @endcode

   in one of your source files.

   \section other_dbs Other databases

   The resp3 protocol is used not only by redis but by other databases as well, for example

   1. KeyDb
   2. ScyllaDB
   3. Redis alternatives.

   Although we test this project against official open source redis only, we
   expected it to work with these databases as well.

 */

/** \file aedis.hpp
 *  \brief Includes all headers that are necessary to use aedis.
 */

