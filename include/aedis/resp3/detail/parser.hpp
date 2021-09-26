/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <string_view>
#include <aedis/resp3/response_adapter_base.hpp>

namespace aedis { namespace resp3 { namespace detail {

// resp3 parser.
class parser {
public:
   enum class bulk_type
   { blob_error
   , verbatim_string 
   , blob_string
   , streamed_string_part
   , none
   };

private:
   response_adapter_base* res_;
   int depth_;
   int sizes_[6]; // Streaming will require a bigger integer.
   bulk_type bulk_;
   int bulk_length_;

   void init(response_adapter_base* res);
   long long on_array_impl(char const* data, int m = 1);
   void on_array(char const* data) { res_->select_array(on_array_impl(data, 1)); }
   void on_push(char const* data) { res_->select_push(on_array_impl(data, 1)); }
   void on_set(char const* data) { res_->select_set(on_array_impl(data, 1)); }
   void on_map(char const* data) { res_->select_map(on_array_impl(data, 2)); }
   void on_attribute(char const* data) { res_->select_attribute(on_array_impl(data, 2)); }
   void on_null();
   void on_simple_string(char const* data, std::size_t n) { res_->on_simple_string(handle_simple_string(data, n)); }
   void on_simple_error(char const* data, std::size_t n) { res_->on_simple_error(handle_simple_string(data, n)); }
   void on_number(char const* data, std::size_t n) { res_->on_number(handle_simple_string(data, n)); }
   void on_double(char const* data, std::size_t n) { res_->on_double(handle_simple_string(data, n)); }
   void on_boolean(char const* data, std::size_t n) { res_->on_bool(handle_simple_string(data, n)); }
   void on_big_number(char const* data, std::size_t n) { res_->on_big_number(handle_simple_string(data, n)); }
   void on_bulk(bulk_type b, std::string_view s = {});
   bulk_type on_blob_error_impl(char const* data, bulk_type b);
   bulk_type on_streamed_string_size(char const* data) { return on_blob_error_impl(data, bulk_type::streamed_string_part); }
   bulk_type on_blob_error(char const* data) { return on_blob_error_impl(data, bulk_type::blob_error); }
   bulk_type on_verbatim_string(char const* data) { return on_blob_error_impl(data, bulk_type::verbatim_string); }
   bulk_type on_blob_string(char const* data);
   std::string_view handle_simple_string(char const* data, std::size_t n);

public:
   parser(response_adapter_base* res);
   std::size_t advance(char const* data, std::size_t n);
   auto done() const noexcept { return depth_ == 0 && bulk_ == bulk_type::none; }
   auto bulk() const noexcept { return bulk_; }
   auto bulk_length() const noexcept { return bulk_length_; }
};

} // detail
} // resp3
} // aedis
