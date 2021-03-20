/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <exception>
#include <string_view>

namespace aedis { namespace resp {

class response_base {
protected:
   virtual void on_simple_string_impl(std::string_view s) { throw std::runtime_error("on_simple_string_impl: Has not been overridden."); }
   virtual void on_simple_error_impl(std::string_view s) { throw std::runtime_error("on_simple_error_impl: Has not been overridden."); }
   virtual void on_number_impl(std::string_view s) { throw std::runtime_error("on_number_impl: Has not been overridden."); }
   virtual void on_double_impl(std::string_view s) { throw std::runtime_error("on_double_impl: Has not been overridden."); }
   virtual void on_null_impl() { throw std::runtime_error("on_null_impl: Has not been overridden."); }
   virtual void on_bool_impl(std::string_view s) { throw std::runtime_error("on_bool_impl: Has not been overridden."); }
   virtual void on_big_number_impl(std::string_view s) { throw std::runtime_error("on_big_number_impl: Has not been overridden."); }
   virtual void on_verbatim_string_impl(std::string_view s = {}) { throw std::runtime_error("on_verbatim_string_impl: Has not been overridden."); }
   virtual void on_blob_string_impl(std::string_view s = {}) { throw std::runtime_error("on_blob_string_impl: Has not been overridden."); }
   virtual void on_blob_error_impl(std::string_view s = {}) { throw std::runtime_error("on_blob_error_impl: Has not been overridden."); }
   virtual void on_streamed_string_part_impl(std::string_view s = {}) { throw std::runtime_error("on_streamed_string_part: Has not been overridden."); }
   virtual void select_array_impl(int n) { throw std::runtime_error("select_array_impl: Has not been overridden."); }
   virtual void select_set_impl(int n) { throw std::runtime_error("select_set_impl: Has not been overridden."); }
   virtual void select_map_impl(int n) { throw std::runtime_error("select_map_impl: Has not been overridden."); }
   virtual void select_push_impl(int n) { throw std::runtime_error("select_push_impl: Has not been overridden."); }
   virtual void select_attribute_impl(int n) { throw std::runtime_error("select_attribute_impl: Has not been overridden."); }

public:
   virtual void pop() {}
   void select_attribute(int n) { select_attribute_impl(n);}
   void select_push(int n) { select_push_impl(n);}
   void select_array(int n) { select_array_impl(n);}
   void select_set(int n) { select_set_impl(n);}
   void select_map(int n) { select_map_impl(n);}
   void on_simple_error(std::string_view s) { on_simple_error_impl(s); }
   void on_blob_error(std::string_view s = {}) { on_blob_error_impl(s); }
   void on_null() { on_null_impl(); }
   void on_simple_string(std::string_view s) { on_simple_string_impl(s); }
   void on_number(std::string_view s) { on_number_impl(s); }
   void on_double(std::string_view s) { on_double_impl(s); }
   void on_bool(std::string_view s) { on_bool_impl(s); }
   void on_big_number(std::string_view s) { on_big_number_impl(s); }
   void on_verbatim_string(std::string_view s = {}) { on_verbatim_string_impl(s); }
   void on_blob_string(std::string_view s = {}) { on_blob_string_impl(s); }
   void on_streamed_string_part(std::string_view s = {}) { on_streamed_string_part_impl(s); }
   virtual ~response_base() {}
};

} // resp
} // aedis

