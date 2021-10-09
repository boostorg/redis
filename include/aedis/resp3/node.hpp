/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <string>
#include <vector>
#include <iostream>

namespace aedis { namespace resp3 {

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

/// Equality compare for a node
bool operator==(node const& a, node const& b);

/// A pre-order-view of the response tree.
using response_impl = std::vector<node>;

} // resp3
} // aedis

std::ostream& operator<<(std::ostream& os, aedis::resp3::node const& o);
std::ostream& operator<<(std::ostream& os, aedis::resp3::response_impl const& r);
