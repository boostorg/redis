/* Copyright (c) 2019 - 2022 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/resp3/node.hpp>

namespace aedis {
namespace resp3 {

std::string to_string(node const& in)
{
   std::string out;
   out += std::to_string(in.depth);
   out += '\t';
   out += to_string(in.data_type);
   out += '\t';
   out += std::to_string(in.aggregate_size);
   out += '\t';
   if (!is_aggregate(in.data_type))
      out += in.data;

   return out;
}

bool operator==(node const& a, node const& b)
{
   return a.aggregate_size == b.aggregate_size
       && a.depth == b.depth
       && a.data_type == b.data_type
       && a.data == b.data;
};

std::ostream& operator<<(std::ostream& os, node const& o)
{
   os << to_string(o);
   return os;
}

std::ostream& operator<<(std::ostream& os, std::vector<node> const& r)
{
   os << to_string(r);
   return os;
}

// TODO: Output like in redis-cli.
std::string to_string(std::vector<node> const& vec)
{
   if (std::empty(vec))
      return {};

   auto begin = std::cbegin(vec);
   std::string res;
   for (; begin != std::prev(std::cend(vec)); ++begin) {
      res += to_string(*begin);
      res += '\n';
   }

   res += to_string(*begin);
   return res;
}
} // resp3
} // aedis
