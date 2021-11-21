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
#include <aedis/resp3/request.hpp>
#include <aedis/resp3/response.hpp>

/** \mainpage Aedis
  
   \section intro_sec Introduction
  
   Aedis is an async redis client built on top of Boost.Asio. It was written
   with emphasis on simplicity and avoid imposing any performance penalty on
   its users.

   \section install_sec Installation

   Aedis is a header only library. You only need to include the header

   @code
   #include <aedis/src.hpp>
   @endcode

   in one of your source files.
 */

/** \file aedis.hpp
 *
 *  Utility header. It includes all headers that are necessary to use
 *  aedis.
 */

