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
#include <aedis/resp3/response_adapter_base.hpp>

namespace aedis {
namespace resp3 {

/** A pre-order-view of the response tree.
 */
class response : public response_adapter_base {
public:
   /** Represents a node in the response tree.
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

   using storage_type = std::vector<node>;
   using value_type = storage_type::value_type;
   using iterator = storage_type::iterator;
   using const_iterator = storage_type::const_iterator;
   using reference = storage_type::reference;
   using const_reference = storage_type::const_reference;
   using pointer = storage_type::pointer;
   using size_type = storage_type::size_type;

private:

   friend
   std::ostream& operator<<(std::ostream& os, response const& r);

   storage_type data_;

public:
   /// Destructor
   virtual ~response() = default;

   auto const& raw() const noexcept {return data_;}
   auto& raw() noexcept {return data_;}

   /// Clears the response but does not release already acquired memory.
   /** Clears the internal buffers but does not release already aquired
    *  memory. This function is usually called before reading a new
    *  response.
    */  
   void clear();

   /// Returns true if the response is empty.
   auto empty() const noexcept { return std::empty(data_);}

   /// Returns the RESP3 type of the response.
   auto get_type() const noexcept { return data_.front().data_type; }

   /// Converts the response to a string.
   std::string
   dump(node::dump_format format = node::dump_format::clean,
	int indent = 3) const;

   /// Returns the begin of aggregate data.
   /** Useful only for flat aggregate types i.e. aggregate that don't 
    *  contain aggregate themselves.
    */
   const_iterator cbegin() const;

   /// Returns the end of aggregate data.
   const_iterator cend() const;
   
   /// Access the element at the specified position.
   const_reference& at(size_type pos) const;

   /// Logical size of the response.
   /** For aggregate data this will be the size of the aggregate, for
    * non-aggregate it will be alsways one.
    */
   size_type size() const noexcept;

   void add(type t, int n, int depth, std::string_view s = {}) override
      { data_.emplace_back(n, depth, t, std::string{s}); }
};

/// Equality comparison for a node.
bool operator==(response::node const& a, response::node const& b);

/** Writes the text representation of node to the output stream.
 *  
 *  NOTE: Binary data is not converted to text.
 */
std::ostream& operator<<(std::ostream& os, response::node const& o);

/** Writes the text representation of the response to the output
 *  stream the response to the output stream.
 */
std::ostream& operator<<(std::ostream& os, response const& r);

} // resp3
} // aedis
