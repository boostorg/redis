/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/resp3/response_adapter_base.hpp>

namespace aedis { namespace resp3 {

struct ignore_adapter : response_adapter_base {
   void on_simple_string(std::string_view s) override {}
   void on_simple_error(std::string_view s) override {}
   void on_number(std::string_view s) override {}
   void on_double(std::string_view s) override {}
   void on_null() override {}
   void on_bool(std::string_view s) override {}
   void on_big_number(std::string_view s) override {}
   void on_verbatim_string(std::string_view s = {}) override {}
   void on_blob_string(std::string_view s = {}) override {}
   void on_blob_error(std::string_view s = {}) override {}
   void on_streamed_string_part(std::string_view s = {}) override {}
   void select_array(int n) override {}
   void select_set(int n) override {}
   void select_map(int n) override {}
   void select_push(int n) override {}
   void select_attribute(int n) override {}
};
} // resp3
} // aedis
