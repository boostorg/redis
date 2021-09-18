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
#include <aedis/response.hpp>
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

// This response type is able to deal with recursive redis responses
// as in a transaction for example.
class array_adapter: public response_adapter_base {
public:
   resp3::array* result;

   array_adapter(resp3::array* p) : result(p) {}

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
   void select_array(int n) override {add_aggregate(n, resp3::type::flat_array);}
   void select_push(int n) override {add_aggregate(n, resp3::type::flat_push);}
   void select_set(int n) override {add_aggregate(n, resp3::type::flat_set);}
   void select_map(int n) override {add_aggregate(n, resp3::type::flat_map);}
   void select_attribute(int n) override {add_aggregate(n, resp3::type::flat_attribute);}

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

struct number_adapter : public response_adapter_base {
   resp3::number* result = nullptr;

   number_adapter(resp3::number* p) : result(p) {}

   void on_number(std::string_view s) override
      { from_string_view(s, *result); }
};

struct blob_string_adapter : public response_adapter_base {
   resp3::blob_string* result = nullptr;

   blob_string_adapter(resp3::blob_string* p) : result(p) {}

   void on_blob_string(std::string_view s) override
      { from_string_view(s, *result); }
};

struct blob_error_adapter : public response_adapter_base {
   resp3::blob_error* result = nullptr;

   blob_error_adapter(resp3::blob_error* p) : result(p) {}

   void on_blob_error(std::string_view s) override
      { from_string_view(s, *result); }
};

struct simple_string_adapter : public response_adapter_base {
   resp3::simple_string* result = nullptr;

   simple_string_adapter(resp3::simple_string* p) : result(p) {}

   void on_simple_string(std::string_view s) override
      { *result = s; }
};

struct simple_error_adapter : public response_adapter_base {
   resp3::simple_error* result = nullptr;

   simple_error_adapter(resp3::simple_error* p) : result(p) {}

   void on_simple_error(std::string_view s) override
      { *result = s; }
};

struct big_number_adapter : public response_adapter_base {
   resp3::big_number* result = nullptr;

   big_number_adapter(resp3::big_number* p) : result(p) {}

   void on_big_number(std::string_view s) override
      { from_string_view(s, *result); }
};

struct doublean_adapter : public response_adapter_base {
   resp3::doublean* result = nullptr;

   doublean_adapter(resp3::doublean* p) : result(p) {}

   void on_double(std::string_view s) override
      { *result = s; }
};

struct verbatim_string_adapter : public response_adapter_base {
   resp3::verbatim_string* result = nullptr;

   verbatim_string_adapter(resp3::verbatim_string* p) : result(p) {}

   void on_verbatim_string(std::string_view s) override
      { from_string_view(s, *result); }
};

struct streamed_string_part_adapter : public response_adapter_base {
   resp3::streamed_string_part* result = nullptr;

   streamed_string_part_adapter(resp3::streamed_string_part* p) : result(p) {}

   void on_streamed_string_part(std::string_view s) override
      { *result += s; }
};

struct boolean_adapter : public response_adapter_base {
   resp3::boolean* result = nullptr;

   boolean_adapter(resp3::boolean* p) : result(p) {}

   void on_bool(std::string_view s) override
   {
      assert(std::ssize(s) == 1);
      *result = s[0] == 't';
   }
};

template <class T>
struct basic_flat_array_adapter : response_adapter_base {
   int i = 0;
   resp3::basic_flat_array<T>* result = nullptr;

   basic_flat_array_adapter(resp3::basic_flat_array<T>* p) : result(p) {}

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

using flat_array_adapter = basic_flat_array_adapter<std::string>;
using flat_array_int_adapter = basic_flat_array_adapter<int>;
using flat_push_adapter = basic_flat_array_adapter<std::string>;

struct flat_map_adapter : response_adapter_base {
   resp3::flat_map* result = nullptr;

   flat_map_adapter(resp3::flat_map* p) : result(p) {}

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

struct flat_set_adapter : response_adapter_base {
   resp3::flat_set* result = nullptr;

   flat_set_adapter(resp3::flat_set* p) : result(p) {}

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

struct response_adapter {
   array_adapter array;
   flat_array_adapter flat_array;
   flat_push_adapter flat_push;
   flat_set_adapter flat_set;
   flat_map_adapter flat_map;
   flat_array_adapter flat_attribute;
   simple_string_adapter simple_string;
   simple_error_adapter simple_error;
   number_adapter number;
   doublean_adapter doublean;
   boolean_adapter boolean;
   big_number_adapter big_number;
   blob_string_adapter blob_string;
   blob_error_adapter blob_error;
   verbatim_string_adapter verbatim_string;
   streamed_string_part_adapter streamed_string_part;
   ignore_adapter resp_ignore;

   response_adapter(response& resp)
   : array{&resp.array}
   , flat_array{&resp.flat_array}
   , flat_push{&resp.flat_push}
   , flat_set{&resp.flat_set}
   , flat_map{&resp.flat_map}
   , flat_attribute{&resp.flat_attribute}
   , simple_string{&resp.simple_string}
   , simple_error{&resp.simple_error}
   , number{&resp.number}
   , doublean{&resp.doublean}
   , boolean{&resp.boolean}
   , big_number{&resp.big_number}
   , blob_string{&resp.blob_string}
   , blob_error{&resp.blob_error}
   , verbatim_string{&resp.verbatim_string}
   , streamed_string_part{&resp.streamed_string_part}
   { }
};

response_adapter_base*
select_adapter(response_adapter& adapter, resp3::type t, command cmd);

} // aedis
