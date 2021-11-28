/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <string>
#include <iostream>
#include <charconv>

#include <aedis/aedis.hpp>

#include "types.hpp"
#include "utils.ipp"

using aedis::command;
using aedis::resp3::type;
using aedis::resp3::request;
using aedis::resp3::response;
using aedis::resp3::async_read;
using aedis::resp3::response_base;

namespace net = aedis::net;

/* Illustrates how to write a custom response.  Useful to users
 *  seeking to improve performance and reduce latency.
 */


/* A response that parses the result of a response directly in an int
   variable thus avoiding unnecessary copies. The same reasoning can
   be applied for keys containing e.g. json strings.
 */
struct response_int : response_base {
   int result;

   void
   add(type t,
       std::size_t aggregate_size,
       std::size_t depth,
       char const* data,
       std::size_t data_size) override
   {
      auto r = std::from_chars(data, data + data_size, result);
      if (r.ec == std::errc::invalid_argument)
         throw std::runtime_error("from_chars: Unable to convert");
   }
};

// To ignore the reponse of a command use the response base class.
using response_ignore = response_base;

/* This coroutine avoids reading the response to a get command in a
   temporary buffer by using a custom response. This is always
   possible when the application knows the data type being stored in a
   specific key.
 */
net::awaitable<void> example()
{
   try {
      request<command> req;
      req.push(command::hello, 3);

      req.push(command::set, "key", 42);
      req.push(command::get, "key");
      req.push(command::quit);

      auto socket = co_await make_connection("127.0.0.1", "6379");
      co_await async_write(socket, req);

      std::string buffer;
      response_ignore ignore;

      // hello
      co_await async_read(socket, buffer, ignore);

      // set
      co_await async_read(socket, buffer, ignore);

      // get
      response_int int_resp;
      co_await async_read(socket, buffer, int_resp);

      std::cout << int_resp.result << std::endl;

      // quit.
      co_await async_read(socket, buffer, ignore);

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}

int main()
{
   net::io_context ioc;
   co_spawn(ioc, example(), net::detached);
   ioc.run();
}

/// \example custom_response1.cpp
