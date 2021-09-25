/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/type.hpp>
#include <aedis/response_adapter_base.hpp>

namespace aedis { namespace resp3 {

struct flat_map_adapter : response_adapter_base {
   flat_map* result = nullptr;

   flat_map_adapter(flat_map* p) : result(p) {}

   void add(std::string_view s = {})
   {
      std::string r;
      r = s;
      result->emplace_back(std::move(r));
   }

   void select_map(int n) override { }

   // We also have to enable arrays, the hello command for example
   // returns a map that has an embeded array.
   // NOTE: Remove this and use map for hello intead of flat_map.
   void select_array(int n) override { }

   void on_simple_string(std::string_view s) override { add(s); }
   void on_number(std::string_view s) override { add(s); }
   void on_double(std::string_view s) override { add(s); }
   void on_bool(std::string_view s) override { add(s); }
   void on_big_number(std::string_view s) override { add(s); }
   void on_verbatim_string(std::string_view s = {}) override { add(s); }
   void on_blob_string(std::string_view s = {}) override { add(s); }
};

} // resp3
} // aedis
