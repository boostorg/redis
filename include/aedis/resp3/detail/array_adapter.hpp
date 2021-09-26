/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/resp3/type.hpp>
#include <aedis/resp3/response_adapter_base.hpp>

namespace aedis { namespace resp3 { namespace detail {

// This response type is able to deal with recursive redis responses
// as in a transaction for example.
class array_adapter: public response_adapter_base {
public:
   array_type* result;

   array_adapter(array_type* p) : result(p) {}

private:
   int depth_ = 0;

   void add_aggregate(int n, type type)
   {
      if (depth_ == 0) {
	 result->reserve(n);
	 ++depth_;
	 return;
      }
      
      result->emplace_back(depth_, type, n);
      result->back().value.reserve(n);
      ++depth_;
   }

   void add(std::string_view s, type type)
   {
      if (std::empty(*result)) {
	 result->emplace_back(depth_, type, 1, command::unknown, std::vector<std::string>{std::string{s}});
      } else if (std::ssize(result->back().value) == result->back().expected_size) {
	 result->emplace_back(depth_, type, 1, command::unknown, std::vector<std::string>{std::string{s}});
      } else {
	 result->back().value.push_back(std::string{s});
      }
   }

public:
   void select_array(int n) override {add_aggregate(n, type::flat_array);}
   void select_push(int n) override {add_aggregate(n, type::flat_push);}
   void select_set(int n) override {add_aggregate(n, type::flat_set);}
   void select_map(int n) override {add_aggregate(n, type::flat_map);}
   void select_attribute(int n) override {add_aggregate(n, type::flat_attribute);}

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
   void clear() { result->clear(); depth_ = 0;}
   auto size() const { return std::size(*result); }
   void pop() override { --depth_; }
};

} // detail
} // resp3
} // aedis
