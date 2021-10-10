/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <string>
#include <iostream>

namespace aedis {
namespace resp3 {

/** Represents a node in the response tree.
 */
struct node {
   /// The number of children node is parent of.
   std::size_t size;

   /// The depth of this node in the response tree.
   std::size_t depth;

   /// The RESP3 type  of the data in this node.
   type data_type;

   /// The data. For aggregate data types this is always empty.
   std::string data;
};

/// Equality comparison for a node.
bool operator==(node const& a, node const& b);

/** Writes the text representation of node to the output stream.
 *  
 *  NOTE: Bonary data is not converted to text.
 */
std::ostream& operator<<(std::ostream& os, node const& o);

} // resp3
} // aedis
