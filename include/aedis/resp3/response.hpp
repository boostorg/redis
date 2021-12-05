/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/command.hpp>
#include <aedis/resp3/type.hpp>
#include <aedis/resp3/request.hpp>

namespace aedis {
namespace resp3 {

/** \brief A general pupose redis response class
 */
struct response {
   /** \brief A node in the response tree.
    */
   struct node {
      enum class dump_format {raw, clean};

      /// The number of children node is parent of.
      std::size_t size;

      /// The depth of this node in the response tree.
      std::size_t depth;

      /// The RESP3 type  of the data in this node.
      type data_type;

      /// The data. For aggregate data types this is always empty.
      std::string data;

      /// Converts the node to a string and appends to out.
      void dump(dump_format format, int indent, std::string& out) const;
   };

   /// The container used to store the response.
   using storage_type = std::vector<node>;

   /// Variable that stores the response in pre-order.
   storage_type result;

   /// Converts the response to a string.
   std::string
   dump(node::dump_format format = node::dump_format::clean,
	int indent = 3) const;

   /** @brief Function called by the parser when new data has been processed.
    *  
    *  Users who what to customize their response types are required
    *  to define this function, see examples.
    *
    *  \param t The RESP3 type of the data.
    *
    *  \param n When t is an aggregate data type this will contain its size
    *     (see also element_multiplicity) for simple data types this is always 1.
    *
    *  \param depth The element depth in the tree.
    *
    *  \param data A pointer to the data.
    *
    *  \param size The size of data.
    */
   void add(type t, std::size_t n, std::size_t depth, char const* data = nullptr, std::size_t size = 0)
      { result.emplace_back(n, depth, t, std::string{data, size}); }
};

/// Equality comparison for a node.
bool operator==(response::node const& a, response::node const& b);

/** \brief Writes node text to the output stream.
 *  
 *  NOTE: Binary data is not converted to text.
 */
std::ostream& operator<<(std::ostream& os, response::node const& o);

/** \brief Writes the response text to the output stream.
 */
std::ostream& operator<<(std::ostream& os, response const& r);

} // resp3
} // aedis
