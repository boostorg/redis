/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/resp3/type.hpp>
#include <aedis/resp3/response_base.hpp>

#include "adapter_utils.hpp"

namespace aedis {
namespace resp3 {
namespace detail {

template <class T>
using basic_flat_array = std::vector<T>;

template <class T>
struct basic_flat_array_adapter : response_base {
   int i = 0;
   basic_flat_array<T>* result = nullptr;

   basic_flat_array_adapter(basic_flat_array<T>* p) : result(p) {}

   void add(type t, int n, int depth, std::string_view s = {}) override
   {
      if (is_aggregate(t)) {
         i = 0;
         result->resize(n);
      } else {
         from_string_view(s, result->at(i));
         ++i;
      }
   }
};

} // detail
} // resp3
} // aedis
