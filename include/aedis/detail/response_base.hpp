/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <exception>
#include <string_view>

#include <aedis/resp_types.hpp>

namespace aedis { namespace resp {

struct response_base {
   virtual void pop() {}
   virtual void on_simple_string(std::string_view s) { throw std::runtime_error("on_simple_string: Has not been overridden."); }
   virtual void on_simple_error(std::string_view s) { throw std::runtime_error("on_simple_error: Has not been overridden."); }
   virtual void on_number(std::string_view s) { throw std::runtime_error("on_number: Has not been overridden."); }
   virtual void on_double(std::string_view s) { throw std::runtime_error("on_double: Has not been overridden."); }
   virtual void on_null() { throw std::runtime_error("on_null: Has not been overridden."); }
   virtual void on_bool(std::string_view s) { throw std::runtime_error("on_bool: Has not been overridden."); }
   virtual void on_big_number(std::string_view s) { throw std::runtime_error("on_big_number: Has not been overridden."); }
   virtual void on_verbatim_string(std::string_view s = {}) { throw std::runtime_error("on_verbatim_string: Has not been overridden."); }
   virtual void on_blob_string(std::string_view s = {}) { throw std::runtime_error("on_blob_string: Has not been overridden."); }
   virtual void on_blob_error(std::string_view s = {}) { throw std::runtime_error("on_blob_error: Has not been overridden."); }
   virtual void on_streamed_string_part(std::string_view s = {}) { throw std::runtime_error("on_streamed_string_part: Has not been overridden."); }
   virtual void select_array(int n) { throw std::runtime_error("select_array: Has not been overridden."); }
   virtual void select_set(int n) { throw std::runtime_error("select_set: Has not been overridden."); }
   virtual void select_map(int n) { throw std::runtime_error("select_map: Has not been overridden."); }
   virtual void select_push(int n) { throw std::runtime_error("select_push: Has not been overridden."); }
   virtual void select_attribute(int n) { throw std::runtime_error("select_attribute: Has not been overridden."); }
   virtual ~response_base() {}
};

} // resp
} // aedis

