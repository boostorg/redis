/* Copyright (c) 2019 - 2022 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/resp3/type.hpp>

#include <string>
#include <vector>

namespace aedis {
namespace adapter {

/** \brief A node in the response tree.
 *  \ingroup any
 *
 *  Redis responses are the pre-order view of the response tree (see
 *  https://en.wikipedia.org/wiki/Tree_traversal#Pre-order,_NLR).
 */
template <class String>
struct node {
   /// The RESP3 type of the data in this node.
  resp3::type data_type;

   /// The number of elements of an aggregate.
   std::size_t aggregate_size;

   /// The depth of this node in the response tree.
   std::size_t depth;

   /// The actual data. For aggregate types this is always empty.
   String value;
};

/** \brief Converts the node to a string.
 *  \ingroup any
 *
 *  \param in The node object.
 */
template <class String>
std::string to_string(node<String> const& in)
{
   std::string out;
   out += std::to_string(in.depth);
   out += '\t';
   out += to_string(in.data_type);
   out += '\t';
   out += std::to_string(in.aggregate_size);
   out += '\t';
   if (!is_aggregate(in.data_type))
      out += in.value;

   return out;
}


/** \brief Compares a node for equality.
 *  \ingroup any
 */
template <class String>
bool operator==(node<String> const& a, node<String> const& b)
{
   return a.aggregate_size == b.aggregate_size
       && a.depth == b.depth
       && a.data_type == b.data_type
       && a.value == b.value;
};

/** \brief Writes the node to the stream.
 *  \ingroup any
 *
 *  NOTE: Binary data is not converted to text.
 */
template <class String>
std::ostream& operator<<(std::ostream& os, node<String> const& o)
{
   os << to_string(o);
   return os;
}


/** \brief Writes the response to the output stream
 *  \ingroup any
 *
 *  TODO: Output like in redis-cli.
 */
template <class String>
std::string to_string(std::vector<node<String>> const& vec)
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

/** \brief Writes the response to the output stream
 *  \ingroup any
 */
template <class String>
std::ostream& operator<<(std::ostream& os, std::vector<node<String>> const& r)
{
   os << to_string(r);
   return os;
}

} // adapter
} // aedis
