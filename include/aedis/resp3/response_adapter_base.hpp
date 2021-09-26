/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <string_view>

namespace aedis { namespace resp3 {

struct response_adapter_base {
   virtual void pop() {}
   virtual void on_simple_string(std::string_view s) { assert(false); }
   virtual void on_simple_error(std::string_view s) { assert(false); }
   virtual void on_number(std::string_view s) { assert(false); }
   virtual void on_double(std::string_view s) { assert(false); }
   virtual void on_null() { assert(false); }
   virtual void on_bool(std::string_view s) { assert(false); }
   virtual void on_big_number(std::string_view s) { assert(false); }
   virtual void on_verbatim_string(std::string_view s = {}) { assert(false); }
   virtual void on_blob_string(std::string_view s = {}) { assert(false); }
   virtual void on_blob_error(std::string_view s = {}) { assert(false); }
   virtual void on_streamed_string_part(std::string_view s = {}) { assert(false); }
   virtual void select_array(int n) { assert(false); }
   virtual void select_set(int n) { assert(false); }
   virtual void select_map(int n) { assert(false); }
   virtual void select_push(int n) { assert(false); }
   virtual void select_attribute(int n) { assert(false); }
   virtual ~response_adapter_base() {}
};

} // resp3
} // aedis
