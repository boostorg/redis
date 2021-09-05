/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <array>
#include <vector>
#include <string>
#include <ostream>
#include <numeric>
#include <type_traits>
#include <string_view>
#include <charconv>
#include <iomanip>

#include <aedis/type.hpp>
#include <aedis/command.hpp>
#include <aedis/response_buffers.hpp>
#include <aedis/response_adapter_base.hpp>

#include <boost/static_string/static_string.hpp>

namespace aedis {

template <class T>
typename std::enable_if<std::is_integral<T>::value, void>::type
from_string_view(std::string_view s, T& n)
{
   auto r = std::from_chars(s.data(), s.data() + s.size(), n);
   if (r.ec == std::errc::invalid_argument)
      throw std::runtime_error("from_chars: Unable to convert");
}

inline
void from_string_view(std::string_view s, std::string& r)
   { r = s; }

struct response_ignore : response_adapter_base {
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

// This response type is able to deal with recursive redis responses
// as in a transaction for example.
class response_tree: public response_adapter_base {
public:
   resp3::transaction* result;

   response_tree(resp3::transaction* p) : result(p) {}

private:
   int depth_ = 0;

   void add_aggregate(int n, resp3::type type)
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

   void add(std::string_view s, resp3::type type)
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
   void select_array(int n) override {add_aggregate(n, resp3::type::array);}
   void select_push(int n) override {add_aggregate(n, resp3::type::push);}
   void select_set(int n) override {add_aggregate(n, resp3::type::set);}
   void select_map(int n) override {add_aggregate(n, resp3::type::map);}
   void select_attribute(int n) override {add_aggregate(n, resp3::type::attribute);}

   void on_simple_string(std::string_view s) override { add(s, resp3::type::simple_string); }
   void on_simple_error(std::string_view s) override { add(s, resp3::type::simple_error); }
   void on_number(std::string_view s) override {add(s, resp3::type::number);}
   void on_double(std::string_view s) override {add(s, resp3::type::doublean);}
   void on_bool(std::string_view s) override {add(s, resp3::type::boolean);}
   void on_big_number(std::string_view s) override {add(s, resp3::type::big_number);}
   void on_null() override {add({}, resp3::type::null);}
   void on_blob_error(std::string_view s = {}) override {add(s, resp3::type::blob_error);}
   void on_verbatim_string(std::string_view s = {}) override {add(s, resp3::type::verbatim_string);}
   void on_blob_string(std::string_view s = {}) override {add(s, resp3::type::blob_string);}
   void on_streamed_string_part(std::string_view s = {}) override {add(s, resp3::type::streamed_string_part);}
   void clear() { result->clear(); depth_ = 0;}
   auto size() const { return std::size(*result); }
   void pop() override { --depth_; }
};

struct response_number : public response_adapter_base {
   resp3::number* result = nullptr;

   response_number(resp3::number* p) : result(p) {}

   void on_number(std::string_view s) override
      { from_string_view(s, *result); }
};

struct response_blob_string : public response_adapter_base {
   resp3::blob_string* result = nullptr;

   response_blob_string(resp3::blob_string* p) : result(p) {}

   void on_blob_string(std::string_view s) override
      { from_string_view(s, *result); }
};

struct response_blob_error : public response_adapter_base {
   resp3::blob_error* result = nullptr;

   response_blob_error(resp3::blob_error* p) : result(p) {}

   void on_blob_error(std::string_view s) override
      { from_string_view(s, *result); }
};

struct response_simple_string : public response_adapter_base {
   resp3::simple_string* result = nullptr;

   response_simple_string(resp3::simple_string* p) : result(p) {}

   void on_simple_string(std::string_view s) override
      { *result = s; }
};

struct response_simple_error : public response_adapter_base {
   resp3::simple_error* result = nullptr;

   response_simple_error(resp3::simple_error* p) : result(p) {}

   void on_simple_error(std::string_view s) override
      { *result = s; }
};

struct response_big_number : public response_adapter_base {
   resp3::big_number* result = nullptr;

   response_big_number(resp3::big_number* p) : result(p) {}

   void on_big_number(std::string_view s) override
      { from_string_view(s, *result); }
};

struct response_double : public response_adapter_base {
   resp3::doublean* result = nullptr;

   response_double(resp3::doublean* p) : result(p) {}

   void on_double(std::string_view s) override
      { *result = s; }
};

struct response_verbatim_string : public response_adapter_base {
   resp3::verbatim_string* result = nullptr;

   response_verbatim_string(resp3::verbatim_string* p) : result(p) {}

   void on_verbatim_string(std::string_view s) override
      { from_string_view(s, *result); }
};

struct response_streamed_string_part : public response_adapter_base {
   resp3::streamed_string_part* result = nullptr;

   response_streamed_string_part(resp3::streamed_string_part* p) : result(p) {}

   void on_streamed_string_part(std::string_view s) override
      { *result += s; }
};

struct response_bool : public response_adapter_base {
   resp3::boolean* result = nullptr;

   response_bool(resp3::boolean* p) : result(p) {}

   void on_bool(std::string_view s) override
   {
      assert(std::ssize(s) == 1);
      *result = s[0] == 't';
   }
};

template <class T>
struct response_basic_array : response_adapter_base {
   int i = 0;
   resp3::basic_array<T>* result = nullptr;

   response_basic_array(resp3::basic_array<T>* p) : result(p) {}

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

using response_array = response_basic_array<std::string>;
using response_array_int = response_basic_array<int>;
using response_push = response_basic_array<std::string>;

struct response_map : response_adapter_base {
   resp3::map* result = nullptr;

   response_map(resp3::map* p) : result(p) {}

   void add(std::string_view s = {})
   {
      std::string r;
      from_string_view(s, r);
      result->emplace_back(std::move(r));
   }

   void select_map(int n) override { }

   // We also have to enable arrays, the hello command for example
   // returns a map that has an embeded array.
   void select_array(int n) override { }

   void on_simple_string(std::string_view s) override { add(s); }
   void on_number(std::string_view s) override { add(s); }
   void on_double(std::string_view s) override { add(s); }
   void on_bool(std::string_view s) override { add(s); }
   void on_big_number(std::string_view s) override { add(s); }
   void on_verbatim_string(std::string_view s = {}) override { add(s); }
   void on_blob_string(std::string_view s = {}) override { add(s); }
};

struct response_set : response_adapter_base {
   resp3::set* result = nullptr;

   response_set(resp3::set* p) : result(p) {}

   void add(std::string_view s = {})
   {
      std::string r;
      from_string_view(s, r);
      result->emplace_back(std::move(r));
   }

   void select_set(int n) override { }

   void on_simple_string(std::string_view s) override { add(s); }
   void on_number(std::string_view s) override { add(s); }
   void on_double(std::string_view s) override { add(s); }
   void on_bool(std::string_view s) override { add(s); }
   void on_big_number(std::string_view s) override { add(s); }
   void on_verbatim_string(std::string_view s = {}) override { add(s); }
   void on_blob_string(std::string_view s = {}) override { add(s); }
};

struct response_adapters {
   response_tree resp_transaction;
   response_array resp_array;
   response_push resp_push;
   response_set resp_set;
   response_map resp_map;
   response_array resp_attribute;
   response_simple_string resp_simple_string;
   response_simple_error resp_simple_error;
   response_number resp_number;
   response_double resp_double;
   response_bool resp_boolean;
   response_big_number resp_big_number;
   response_blob_string resp_blob_string;
   response_blob_error resp_blob_error;
   response_verbatim_string resp_verbatim_string;
   response_streamed_string_part resp_streamed_string_part;
   response_ignore resp_ignore;

   response_adapters(response_buffers& buf)
   : resp_transaction{&buf.transaction}
   , resp_array{&buf.array}
   , resp_push{&buf.push}
   , resp_set{&buf.set}
   , resp_map{&buf.map}
   , resp_attribute{&buf.attribute}
   , resp_simple_string{&buf.simple_string}
   , resp_simple_error{&buf.simple_error}
   , resp_number{&buf.number}
   , resp_double{&buf.doublean}
   , resp_boolean{&buf.boolean}
   , resp_big_number{&buf.big_number}
   , resp_blob_string{&buf.blob_string}
   , resp_blob_error{&buf.blob_error}
   , resp_verbatim_string{&buf.verbatim_string}
   , resp_streamed_string_part{&buf.streamed_string_part}
   { }
};

} // aedis
