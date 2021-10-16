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
   o.dump(response::node::dump_format::clean, 3, res);
   os << res;
   return os;
}

std::ostream& operator<<(std::ostream& os, response const& r)
{
   os << r.dump();
   return os;
}

void response::node::dump(dump_format format, int indent, std::string& out) const
{
   switch (format) {
      case response::node::dump_format::raw:
      {
	 out += std::to_string(depth);
	 out += '\t';
	 out += to_string(data_type);
	 out += '\t';
	 out += data;
      } break;

      case response::node::dump_format::clean:
      {
	 std::string prefix(indent * depth, ' ');
	 out += prefix;
	 out += data;
      } break;

      default: {
      }
   }
}

std::string
response::dump(node::dump_format format, int indent) const
{
   std::string res;
   for (auto const& n : data_) {
      if (n.size > 1)
	 continue;
      n.dump(format, indent, res);
      res += '\n';
   }

   return res;
}

} // resp3
} // aedis
