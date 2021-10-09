/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

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
   response_impl* result_;
   std::size_t depth_;

   void add_aggregate(int n, type t)
   {
      result_->emplace_back(n, depth_, t, std::string{});
      ++depth_;
   }

   void add(std::string_view s, type t)
      { result_->emplace_back(1, depth_, t, std::string{s}); }

public:
   array_adapter(response_impl* p = nullptr)
   : result_{p}
   , depth_{0}
   { }

   void select_array(int n) override {add_aggregate(n, type::array);}
   void select_push(int n) override {add_aggregate(n, type::push);}
   void select_set(int n) override {add_aggregate(n, type::set);}
   void select_map(int n) override {add_aggregate(n, type::map);}
   void select_attribute(int n) override {add_aggregate(n, type::attribute);}

   void on_simple_string(std::string_view s) override { add(s, type::simple_string); }
   void on_simple_error(std::string_view s) override { add(s, type::simple_error); }
   void on_number(std::string_view s) override {add(s, type::number);}
   void on_double(std::string_view s) override {add(s, type::doublean);}
   void on_bool(std::string_view s) override {add(s, type::boolean);}
   void on_big_number(std::string_view s) override {add(s, type::big_number);}
   void on_null() override {add({}, type::null);}
   void on_blob_error(std::string_view s = {}) override {add(s, type::blob_error);}
   void on_verbatim_string(std::string_view s = {}) override {add(s, type::verbatim_string);}
   void on_blob_string(std::string_view s = {}) override {add(s, type::blob_string);}
   void on_streamed_string_part(std::string_view s = {}) override {add(s, type::streamed_string_part);}
   void pop() override { --depth_; }
   void clear() { depth_ = 0; }
};

} // detail
} // resp3
} // aedis
