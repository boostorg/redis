/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/resp3/type.hpp>
#include <aedis/resp3/response_adapter_base.hpp>
#include <aedis/resp3/detail/adapter_utils.hpp>

namespace aedis { namespace resp3 { namespace detail {

template <class T>
using basic_flat_array = std::vector<T>;

template <class T>
struct basic_flat_array_adapter : response_adapter_base {
   int i = 0;
   basic_flat_array<T>* result = nullptr;

   basic_flat_array_adapter(basic_flat_array<T>* p) : result(p) {}

   void add(std::string_view s = {})
   {
      from_string_view(s, result->at(i));
      ++i;
   }

   void select_array(int n) override
   {
      i = 0;
      result->resize(n);
   }

   void select_push(int n) override
   {
      i = 0;
      result->resize(n);
   }

   // TODO: Call vector reserve.
   void on_simple_string(std::string_view s) override { add(s); }
   void on_number(std::string_view s) override { add(s); }
   void on_double(std::string_view s) override { add(s); }
   void on_bool(std::string_view s) override { add(s); }
   void on_big_number(std::string_view s) override { add(s); }
   void on_verbatim_string(std::string_view s = {}) override { add(s); }
   void on_blob_string(std::string_view s = {}) override { add(s); }
   void select_set(int n) override { }
   void select_map(int n) override { }
   void on_streamed_string_part(std::string_view s = {}) override { add(s); }
};

} // detail
} // resp3
} // aedis
