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

namespace net = aedis::net;

/* \brief An example response class for aggregate data types.
  
    Similar to custon_response1 but handles aggregagate data types.
  
    Instead of reading a response into a std::vector<std::string> and
    then converting to a std::vector<int> we parse the ints form the
    read buffer directly into an int.
 */

/* A response type that parses the response directly in a
 *  std::vector<int>
 */
class response_vector {
private:
   int i_ = 0;

public:
   std::vector<int> result;

   void
   add(type t,
       std::size_t aggregate_size,
       std::size_t depth,
       char const* data,
       std::size_t data_size)
   {
      if (is_aggregate(t)) {
         auto const m = element_multiplicity(t);
         result.resize(m * aggregate_size);
      } else {
         auto r = std::from_chars(data, data + data_size, result.at(i_));
         if (r.ec == std::errc::invalid_argument)
            throw std::runtime_error("from_chars: Unable to convert");
         ++i_;
      }
   }
};

struct response_ignore {

   void
   add(type t,
       std::size_t aggregate_size,
       std::size_t depth,
       char const* data = nullptr,
       std::size_t data_size = 0u) { }
};


net::awaitable<void> ping()
{
   try {
      request<command> req;
      req.push(command::hello, 3);

      std::vector<int> vec {1, 2, 3, 4, 5, 6};
      req.push_range(command::rpush, "key2", std::cbegin(vec), std::cend(vec));

      req.push(command::lrange, "key2", 0, -1);
      req.push(command::quit);

      auto socket = co_await make_connection("127.0.0.1", "6379");
      co_await async_write(socket, req);

      std::string buffer;
      response_ignore ignore;

      // hello
      co_await async_read(socket, buffer, ignore);

      // rpush
      co_await async_read(socket, buffer, ignore);

      // lrange
      response_vector vec_resp;
      co_await async_read(socket, buffer, vec_resp);

      for (auto e: vec_resp.result)
         std::cout << e << std::endl;

      // quit.
      co_await async_read(socket, buffer, ignore);

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}

int main()
{
   net::io_context ioc;
   co_spawn(ioc, ping(), net::detached);
   ioc.run();
}

/// \example custom_response2.cpp
