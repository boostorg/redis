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
   void on_aggregate(type t, char const* data);
   void on_null();
   void on_data(type t, char const* data, std::size_t n);
   void on_bulk(bulk_type b, std::string_view s = {});
   bulk_type on_blob_error(char const* data, bulk_type b);
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
