/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/resp3/type.hpp>

#include <string_view>

namespace aedis {
namespace resp3 {

/** Response adapter base class.
 *
 *  Users are allowed to override this class.
 */
struct response_adapter_base {
   /** Called by the parser when it is done at the specific depth. In
    * other words, when it is done with an aggregate data type.
    */
   virtual void pop() {}

   /** Called in the parser everytime a simple (non-aggregate) data
    * type arrives, those are
    *
    *    simple_string
    *    simple_error
    *    number
    *    null
    *    double
    *    bool
    *    big_number
    *    blob_error
    *    blob_string
    *    verbatim_string
    *    streamed_string_part
    */
   virtual void add(type t, std::string_view s = {}) {}

   /** Called from the parser everytime a new RESP3 aggregate data
    *  type is received, those are
    *
    *    array
    *    push
    *    set
    *    map
    *    attribute
    */
   virtual void add_aggregate(type t, int n) { }

   /// Destructor.
   virtual ~response_adapter_base() {}
};

} // resp3
} // aedis
