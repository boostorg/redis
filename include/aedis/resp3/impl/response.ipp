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

bool operator==(node const& a, node const& b)
{
   return a.size == b.size
       && a.depth == b.depth
       && a.data_type == b.data_type
       && a.data == b.data;
};

std::ostream& operator<<(std::ostream& os, node const& o)
{
   std::string res;
   o.dump(node::dump_format::clean, 3, res);
   os << res;
   return os;
}

std::ostream& operator<<(std::ostream& os, storage_type const& r)
{
   os << dump(r);
   return os;
}

void node::dump(dump_format format, int indent, std::string& out) const
{
   switch (format) {
      case node::dump_format::raw:
      {
	 out += std::to_string(depth);
	 out += '\t';
	 out += to_string(data_type);
	 out += '\t';
	 out += std::to_string(size);
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
	    if (size == 0) {
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

} // resp3
} // aedis
