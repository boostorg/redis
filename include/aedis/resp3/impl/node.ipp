/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/resp3/node.hpp>

namespace aedis { namespace resp3 {

bool operator==(node const& a, node const& b)
{
   return a.size == b.size
       && a.depth == b.depth
       && a.data_type == b.data_type
       && a.data == b.data;
};

} // resp3
} // aedis

std::ostream& operator<<(std::ostream& os, aedis::resp3::node const& o)
{
   std::string res(3 * o.depth, ' ');
   res += o.data;
   res += "(";
   res += to_string(o.data_type);
   res += ")";
   os << res;
   return os;
}

std::ostream& operator<<(std::ostream& os, aedis::resp3::response_impl const& r)
{
   for (auto const& n : r)
      os << n << "\n";

   return os;
}
