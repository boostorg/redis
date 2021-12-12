/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/resp3/adapt.hpp>
#include <aedis/command.hpp>

namespace aedis {
namespace resp3 {

std::string dump(storage_type const& obj, node::dump_format format, int indent)
{
   if (std::empty(obj))
      return {};

   std::string res;
   for (auto i = 0ULL; i < std::size(obj) - 1; ++i) {
      obj[i].dump(format, indent, res);
      res += '\n';
   }

   obj.back().dump(format, indent, res);
   return res;
}

std::ostream& operator<<(std::ostream& os, storage_type const& r)
{
   os << dump(r);
   return os;
}

} // resp3
} // aedis
