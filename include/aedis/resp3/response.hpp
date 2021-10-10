/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/command.hpp>
#include <aedis/resp3/type.hpp>
#include <aedis/resp3/response_adapter_base.hpp>

namespace aedis {
namespace resp3 {

/// A pre-order-view of the response tree.
class response {
public:
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

   using storage_type = std::vector<node>;

public:
   // This response type is able to deal with recursive redis responses
   // as in a transaction for example.
   class adapter: public response_adapter_base {
   private:
      storage_type* result_;
      std::size_t depth_;

   public:
      adapter(storage_type* p)
      : result_{p}
      , depth_{0}
      { }

      void add_aggregate(type t, int n) override
	 { result_->emplace_back(n, depth_, t, std::string{}); ++depth_; }

      void add(type t, std::string_view s = {}) override
	 { result_->emplace_back(1, depth_, t, std::string{s}); }

      void pop() override
	 { --depth_; }

      void clear()
	 { depth_ = 0; }
   };
   private:

   friend
   std::ostream& operator<<(std::ostream& os, response const& r);

   storage_type data_;
   adapter adapter_{&data_};


public:
   /// Destructor
   virtual ~response() = default;

   /** Returns the response adapter suitable to construct this
    *  response type from the wire format. Override this function for
    *  your own response types.
    */
   virtual adapter*
   select_adapter(
      resp3::type t,
      command cmd = command::unknown,
      std::string const& key = "")
      { return &adapter_; }


   auto const& raw() const noexcept {return data_;}
   auto& raw() noexcept {return data_;}

   /** Clears the internal buffers but does not release already aquired
    *  memory. This function is usually called before reading a new
    *  response.
    */  
   void clear();

   /// Returns true if the response is empty.
   auto empty() const noexcept { return std::empty(data_);}

   /** Returns the RESP3 type of the response.
    *  
    *  Expects a non-empty response.
    */
   auto get_type() const noexcept { return data_.front().data_type; }
};

/// Equality comparison for a node.
bool operator==(response::node const& a, response::node const& b);

/** Writes the text representation of node to the output stream.
 *  
 *  NOTE: Bonary data is not converted to text.
 */
std::ostream& operator<<(std::ostream& os, response::node const& o);

/** Writes the text representation of the response to the output
 *  stream the response to the output stream.
 */
std::ostream& operator<<(std::ostream& os, response const& r);

} // resp3
} // aedis
