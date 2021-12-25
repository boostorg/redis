/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/resp3/node.hpp>

namespace aedis {
namespace resp3 {

void node::dump(std::string& out, dump_format format, int indent) const
{
   switch (format) {
      case node::dump_format::raw:
      {
	 out += std::to_string(depth);
	 out += '\t';
	 out += to_string(data_type);
	 out += '\t';
	 out += std::to_string(aggregate_size);
	 out += '\t';
	 if (!is_aggregate(data_type))
	    out += data;
      } break;
      case node::dump_format::clean:
      {
	 std::string prefix(indent * depth, ' ');
	 out += prefix;
	 if (is_aggregate(data_type)) {
	    out += "(";
	    out += to_string(data_type);
	    out += ")";
	    if (aggregate_size == 0) {
	       std::string prefix2(indent * (depth + 1), ' ');
	       out += "\n";
	       out += prefix2;
	       out += "(empty)";
	    }
	 } else {
	    if (std::empty(data))
	       out += "(empty)";
	    else
	       out += data;
	 }
      } break;
      default: { }
   }
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
   std::string res;
   o.dump(res, node::dump_format::clean, 3);
   os << res;
   return os;
}

std::ostream& operator<<(std::ostream& os, std::vector<node> const& r)
{
   os << dump(std::cbegin(r), std::cend(r));
   return os;
}

} // resp3
} // aedis
