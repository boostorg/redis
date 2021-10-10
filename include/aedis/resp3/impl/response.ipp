/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/resp3/response.hpp>
#include <aedis/command.hpp>

namespace aedis {
namespace resp3 {

void response::clear()
{
   data_.resize(0);
   adapter_.clear();
}

bool operator==(response::node const& a, response::node const& b)
{
   return a.size == b.size
       && a.depth == b.depth
       && a.data_type == b.data_type
       && a.data == b.data;
};

std::ostream& operator<<(std::ostream& os, response::node const& o)
{
   std::string res;
   res += std::to_string(o.depth);
   res += '\t';
   res += to_string(o.data_type);
   res += '\t';
   res += o.data;
   os << res;
   return os;
}

std::ostream& operator<<(std::ostream& os, response const& r)
{
   for (auto const& n : r.data_)
      os << n << "\n";

   return os;
}

} // resp3
} // aedis
