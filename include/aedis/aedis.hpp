/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

/** \mainpage My Personal Index Page
  
   \section intro_sec Introduction
  
   Aedis is an async redis client built on top of Boost.Asio. It was written with emphasis
   on simplicity and avoid imposing any performance penalty on its users.

   The recommended useage looks like the code below.

   \code{.cpp}

   std::queue<resp3::request> requests;
   requests.push({});
   requests.back().push(command::hello, 3);
   ...

   resp3::stream<tcp_socket> stream{std::move(socket)};
   for (;;) {
      resp3::response resp;
      co_await stream.async_consume(requests, resp);

      if (resp.get_type() == resp3::type::push) {
         continue; // Push type received. Do something and continue.
      }

      switch (requests.front().commands.front()) {
        case command::hello:
        {
           prepare_next(requests);
           requests.back().push(command::publish, "channel1", "Message to channel1");
           requests.back().push(command::publish, "channel2", "Message to channel2");
        } break;
        // ...
      }
   }

   \endcode

   The function async_consume will write any out outstanding command in the queue if needed. That
   makes it possible for users to add new commands without havin to write them explicitly. For example
  
   \section install_sec Installation
  
   \subsection step1 Step 1: Opening the box
  
   etc...
 */

/** \file aedis.hpp
 *
 *  Utility header. It includes all headers that are necessary to use
 *  aedis.
 */

#include <aedis/version.hpp>
#include <aedis/resp3/request.hpp>
#include <aedis/resp3/stream.hpp>
#include <aedis/resp3/response.hpp>
