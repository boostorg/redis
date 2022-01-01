/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/resp3/type.hpp>

#include <string>

namespace aedis {
namespace resp3 {

/** \brief A node in the response tree.
 *  \ingroup classes
 */
struct node {
   enum class dump_format {raw, clean};

   /// The RESP3 type  of the data in this node.
   type data_type;

   /// The number of children this node is parent of.
   std::size_t aggregate_size;

   /// The depth of this node in the response tree.
   std::size_t depth;

   /// The actual data. For aggregate data types this is always empty.
   std::string data;

   /// Converts the node to a string and appends to out.
   void dump(std::string& out, dump_format format = dump_format::raw, int indent = 3) const;
};

/** \ingroup functions
 *  @{
 */

/// Compares a node for equality.
bool operator==(node const& a, node const& b);

/// Writes the node to the stream.
std::ostream& operator<<(std::ostream& os, node const& o);

template <class ForwardIterator>
std::string dump(
   ForwardIterator begin,
   ForwardIterator end,
   node::dump_format format = node::dump_format::clean,
   int indent = 3)
{
   if (begin == end)
      return {};

   std::string res;
   for (; begin != std::prev(end); ++begin) {
      begin->dump(res, format, indent);
      res += '\n';
   }

   begin->dump(res, format, indent);
   return res;
}

/// Equality comparison for a node.
bool operator==(node const& a, node const& b);

/** Writes the text representation of node to the output stream.
 *  
 *  NOTE: Binary data is not converted to text.
 */
std::ostream& operator<<(std::ostream& os, node const& o);

/** Writes the text representation of the response to the output
 *  stream the response to the output stream.
 */
std::ostream& operator<<(std::ostream& os, std::vector<node> const& r);

/*! @} */

} // resp3
} // aedis
