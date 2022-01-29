/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/aedis.hpp>

namespace net = aedis::net;
using aedis::redis::command;
using aedis::resp3::adapt;
using aedis::resp3::response_traits;
using aedis::resp3::type;
using aedis::resp3::node;

// Groups the responses used in the examples.
struct responses {
   int number;
   std::string simple_string;
   std::vector<node> general;
};

// Adpter as required by experimental::client.
class adapter_wrapper {
private:
   response_traits<int>::adapter_type number_adapter_;
   response_traits<std::string>::adapter_type str_adapter_;
   response_traits<std::vector<node>>::adapter_type general_adapter_;

public:
   adapter_wrapper(responses& resps)
   : number_adapter_{adapt(resps.number)}
   , str_adapter_{adapt(resps.simple_string)}
   , general_adapter_{adapt(resps.general)}
   {}

   void
   operator()(
      command cmd,
      type t,
      std::size_t aggregate_size,
      std::size_t depth,
      char const* data,
      std::size_t size,
      std::error_code& ec)
   {
      // Handles only the commands we are interested in the examples and
      // ignores the rest.
      switch (cmd) {
         case command::quit:
         case command::ping:
         str_adapter_(t, aggregate_size, depth, data, size, ec);
         return;

         case command::incr:
         number_adapter_(t, aggregate_size, depth, data, size, ec);
         return;

         case command::unknown:
         general_adapter_(t, aggregate_size, depth, data, size, ec);
         return;

         default: {} // Ignore.
      }
   }
};

