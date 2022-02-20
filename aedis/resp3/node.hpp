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
namespace resp3 {

/** \brief A node in the response tree.
 *  \ingroup any
 *
 *  Redis responses are the pre-order view of the response tree (see
 *  https://en.wikipedia.org/wiki/Tree_traversal#Pre-order,_NLR).
 */
struct node {
   /// The RESP3 type of the data in this node.
   type data_type;

   /// The number of elements of an aggregate.
   std::size_t aggregate_size;

   /// The depth of this node in the response tree.
   std::size_t depth;

   /// The actual data. For aggregate data types this is always empty.
   std::string data; // TODO: rename to value.
};

/** \brief Converts the node to a string.
 *  \ingroup any
 *
 *  \param obj The node object.
 */
std::string to_string(node const& obj);

/** \brief Compares a node for equality.
 *  \ingroup any
 */
bool operator==(node const& a, node const& b);

/** \brief Writes the node to the stream.
 *  \ingroup any
 *
 *  NOTE: Binary data is not converted to text.
 */
std::ostream& operator<<(std::ostream& os, node const& o);

/** \brief Writes the response to the output stream
 *  \ingroup any
 */
std::string to_string(std::vector<node> const& vec);

/** \brief Writes the response to the output stream
 *  \ingroup any
 */
std::ostream& operator<<(std::ostream& os, std::vector<node> const& r);

} // resp3
} // aedis
