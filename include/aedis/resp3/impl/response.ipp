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
	 out += std::to_string(size);
	 out += '\t';
	 if (!is_aggregate(data_type))
	    out += data;
      } break;
      case response::node::dump_format::clean:
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

std::string response::dump(node::dump_format format, int indent) const
{
   if (std::empty(data_))
      return {};

   std::string res;
   for (auto i = 0ULL; i < std::size(data_) - 1; ++i) {
      data_[i].dump(format, indent, res);
      res += '\n';
   }

   data_.back().dump(format, indent, res);
   return res;
}


response::const_iterator response::cbegin() const
{
   assert(!std::empty(data_));
   assert(is_aggregate(data_.front().data_type));

   return std::cbegin(data_) + 1;
}

response::const_iterator response::cend() const
{
   auto size = data_.front().size;
   if (is_aggregate(data_.front().data_type))
      size *= 2;

   return cbegin() + size;
}

response::const_reference response::at(size_type pos) const
{
   if (is_aggregate(data_.front().data_type))
      return data_.at(pos + 1);

   return data_.at(pos);
}

response::size_type response::size() const noexcept
{
   if (is_aggregate(data_.front().data_type)) {
      assert(std::size(data_) >= 1);
      auto const logical_size = std::size(data_) - 1;
      assert(data_.front().size == logical_size);
      return data_.front().size;
   }

   return 1;
}

} // resp3
} // aedis
