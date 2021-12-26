/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/resp3/detail/composer.hpp>

namespace aedis {
namespace resp3 {
namespace detail {

void add_header(std::string& to, int size)
{
   to += "*";
   to += std::to_string(size);
   to += "\r\n";
}

void add_bulk(std::string& to, std::string_view data)
{
   to += "$";
   to += std::to_string(std::size(data));
   to += "\r\n";
   to += data;
   to += "\r\n";
}

bool has_push_response(command cmd)
{
   switch (cmd) {
      case command::subscribe:
      case command::unsubscribe:
      case command::psubscribe:
      return true;

      default:
      return false;
   }
};

} // detail
} // resp3
} // aedis
