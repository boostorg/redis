/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/command.hpp>
#include <aedis/resp3/type.hpp>
#include <aedis/resp3/node.hpp>
#include <aedis/resp3/detail/array_adapter.hpp>

namespace aedis { namespace resp3 {

/// A pre-order-view of the response tree.
class response {
public:
   using storage_type = std::vector<node>;

private:
   friend
   std::ostream& operator<<(std::ostream& os, response const& r);

   storage_type data_;
   detail::array_adapter array_adapter_{&data_};

public:
   virtual ~response() = default;

   /** Returns the response adapter suitable to construct this
    *  response type from the wire format. Override this function for
    *  your own response types.
    */
   virtual
   response_adapter_base*
   select_adapter(
      resp3::type type,
      command cmd = command::unknown,
      std::string const& key = "");

   auto const& raw() const noexcept {return data_;}
   auto& raw() noexcept {return data_;}
};

/** Writes the text representation of the response to the output
 *  stream the response to the output stream.
 */
std::ostream& operator<<(std::ostream& os, response const& r);

} // resp3
} // aedis
