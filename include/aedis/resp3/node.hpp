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
 *  \ingroup classes
 */
struct node {
   /// The RESP3 type of the data in this node.
   type data_type;

   /// The number of children this node is parent of.
   std::size_t aggregate_size;

   /// The depth of this node in the response tree.
   std::size_t depth;

   /// The actual data. For aggregate data types this is always empty.
   std::string data;
};

/** \brief Converts the node to a string.
 *  \ingroup functions
 */
std::string to_string(node const& obj);

/** \brief Compares a node for equality.
 *  \ingroup operators
 */
bool operator==(node const& a, node const& b);

/** \brief Writes the node to the stream.
 *  \ingroup operators
 *
 *  NOTE: Binary data is not converted to text.
 */
std::ostream& operator<<(std::ostream& os, node const& o);

/** \brief Writes the response to the output stream
 *  \ingroup functions
 */
std::string to_string(std::vector<node> const& vec);

/** \brief Writes the response to the output stream
 *  \ingroup operators
 */
std::ostream& operator<<(std::ostream& os, std::vector<node> const& r);

} // resp3
} // aedis
