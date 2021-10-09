/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <vector>

#include <aedis/resp3/type.hpp>
#include <aedis/resp3/node.hpp>
#include <aedis/resp3/response_adapter_base.hpp>

namespace aedis {
namespace resp3 {
namespace detail {

// This response type is able to deal with recursive redis responses
// as in a transaction for example.
class array_adapter: public response_adapter_base {
private:
   std::vector<node>* result_;
   std::size_t depth_;

public:
   array_adapter(std::vector<node>* p = nullptr)
   : result_{p}
   , depth_{0}
   { }

   void add_aggregate(type t, int n) override
   {
      result_->emplace_back(n, depth_, t, std::string{});
      ++depth_;
   }

   void add(type t, std::string_view s = {}) override
      { result_->emplace_back(1, depth_, t, std::string{s}); }

   void pop() override { --depth_; }
   void clear() { depth_ = 0; }
};

} // detail
} // resp3
} // aedis
