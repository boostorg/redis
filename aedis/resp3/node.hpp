/* Copyright (c) 2018 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_RESP3_NODE_HPP
#define AEDIS_RESP3_NODE_HPP

#include <aedis/resp3/type.hpp>

#include <string>
#include <vector>

namespace aedis {
namespace resp3 {

/** \brief A node in the response tree.
 *  \ingroup any
 *
 *  Redis responses are the pre-order view of the response tree (see
 *  https://en.wikipedia.org/wiki/Tree_traversal#Pre-order,_NLR).
 *
 *  The node class represent one element in the response tree. The string type
 *  is a template give more flexibility, for example
 *
 *  @li @c boost::string_view
 *  @li @c std::string
 *  @li @c boost::static_string
 *
 *  \remark Any Redis response can be received in an array of nodes, for
 *  example \c std::vector<node<std::string>>.
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
      out.append(in.value.data(), in.value.size());

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

} // adapter
} // aedis

#endif // AEDIS_RESP3_NODE_HPP
