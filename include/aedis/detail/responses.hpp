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

#include <aedis/detail/command.hpp>
#include <aedis/config.hpp>
#include <aedis/type.hpp>
#include <aedis/resp_types.hpp>

#include "response_base.hpp"

#include <boost/static_string/static_string.hpp>

namespace aedis { namespace resp {

template <class T>
std::enable_if<std::is_integral<T>::value, void>::type
from_string_view(std::string_view s, T& n)
{
   auto r = std::from_chars(s.data(), s.data() + s.size(), n);
   if (r.ec == std::errc::invalid_argument)
      throw std::runtime_error("from_chars: Unable to convert");
}

inline
void from_string_view(std::string_view s, std::string& r)
   { r = s; }

struct response_ignore : response_base {
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
class response_tree: public response_base {
public:
   struct elem {
      int depth;
      type t;
      int expected_size = -1;
      std::vector<std::string> value;
   };

   std::vector<elem> result;

private:
   int depth_ = 0;

   void add_aggregate(int n, type t)
   {
      if (depth_ == 0) {
	 result.reserve(n);
	 ++depth_;
	 return;
      }
      
      result.emplace_back(depth_, t, n);
      result.back().value.reserve(n);
      ++depth_;
   }

   void add(std::string_view s, type t)
   {
      if (std::empty(result)) {
	 result.emplace_back(depth_, t, 1, std::vector<std::string>{std::string{s}});
      } else if (std::ssize(result.back().value) == result.back().expected_size) {
	 result.emplace_back(depth_, t, 1, std::vector<std::string>{std::string{s}});
      } else {
	 result.back().value.push_back(std::string{s});
      }
   }

public:
   void select_array(int n) override {add_aggregate(n, type::array);}
   void select_push(int n) override {add_aggregate(n, type::push);}
   void select_set(int n) override {add_aggregate(n, type::set);}
   void select_map(int n) override {add_aggregate(n, type::map);}
   void select_attribute(int n) override {add_aggregate(n, type::attribute);}

   void on_simple_string(std::string_view s) override { add(s, type::simple_string); }
   void on_simple_error(std::string_view s) override { add(s, type::simple_error); }
   void on_number(std::string_view s) override {add(s, type::number);}
   void on_double(std::string_view s) override {add(s, type::double_type);}
   void on_bool(std::string_view s) override {add(s, type::boolean);}
   void on_big_number(std::string_view s) override {add(s, type::big_number);}
   void on_null() override {add({}, type::null);}
   void on_blob_error(std::string_view s = {}) override {add(s, type::blob_error);}
   void on_verbatim_string(std::string_view s = {}) override {add(s, type::verbatim_string);}
   void on_blob_string(std::string_view s = {}) override {add(s, type::blob_string);}
   void on_streamed_string_part(std::string_view s = {}) override {add(s, type::streamed_string_part);}
   void clear() { result.clear(); depth_ = 0;}
   auto size() const { return result.size(); }
   void pop() override { --depth_; }
};

struct response_number : public response_base {
   void on_number(std::string_view s) override
      { from_string_view(s, result); }

   number_type result;
};

template<
   class CharT,
   class Traits = std::char_traits<CharT>,
   class Allocator = std::allocator<CharT>>
struct response_basic_blob_string : public response_base {
   void on_blob_string(std::string_view s) override
      { from_string_view(s, result); }

   basic_blob_string<char> result;
};

template<
   class CharT = char,
   class Traits = std::char_traits<CharT>,
   class Allocator = std::allocator<CharT>>
struct response_basic_blob_error : public response_base {
   void on_blob_error(std::string_view s) override
      { from_string_view(s, result); }

   basic_blob_error<char> result;
};

template<
   class CharT = char,
   class Traits = std::char_traits<CharT>,
   class Allocator = std::allocator<CharT>
   >
struct response_basic_simple_string : public response_base {
   void on_simple_string(std::string_view s) override
      { from_string_view(s, result); }

   basic_simple_string<CharT, Traits, Allocator> result;
};

template<
   class CharT = char,
   class Traits = std::char_traits<CharT>,
   class Allocator = std::allocator<CharT>
   >
struct response_basic_simple_error : public response_base {
   void on_simple_error(std::string_view s) override
      { from_string_view(s, result); }

   basic_simple_error<CharT, Traits, Allocator> result;
};

// Big number uses strings at the moment as the underlying storage.
template <
   class CharT = char,
   class Traits = std::char_traits<CharT>,
   class Allocator = std::allocator<CharT>
   >
struct response_basic_big_number : public response_base {
   void on_big_number(std::string_view s) override
      { from_string_view(s, result); }

   basic_big_number<CharT, Traits, Allocator> result;
};

struct response_double : public response_base {
   void on_double(std::string_view s) override
   {
      result = s;
   }

   double_type result;
};

template<
   class CharT = char,
   class Traits = std::char_traits<CharT>,
   class Allocator = std::allocator<CharT>
   >
struct response_basic_verbatim_string : public response_base {
   void on_verbatim_string(std::string_view s) override
      { from_string_view(s, result); }
public:
   basic_verbatim_string<CharT, Traits, Allocator> result;
};

template<
   class CharT = char,
   class Traits = std::char_traits<CharT>,
   class Allocator = std::allocator<CharT>
   >
struct response_basic_streamed_string_part : public response_base {
   void on_streamed_string_part(std::string_view s) override
      { result += s; }

   basic_streamed_string_part<CharT, Traits, Allocator> result;
};

struct response_bool : public response_base {
   void on_bool(std::string_view s) override
   {
      assert(std::ssize(s) == 1);
      result = s[0] == 't';
   }

   bool_type result;
};

template<
   class Key,
   class Compare = std::less<Key>,
   class Allocator = std::allocator<Key>
   >
struct response_unordered_set : response_base {
   void on_blob_string(std::string_view s) override
   {
      Key r;
      from_string_view(s, r);
      result.insert(std::end(result), std::move(r));
   }

   void select_array(int n) override { }
   void select_set(int n) override { }

   std::set<Key, Compare, Allocator> result;
};

template <
   class T,
   class Allocator = std::allocator<T>
   >
struct response_basic_array : response_base {
   void add(std::string_view s = {})
   {
      T r;
      from_string_view(s, r);
      result.emplace_back(std::move(r));
   }

   // TODO: Call vector reserver.
   void on_simple_string(std::string_view s) override { add(s); }
   void on_number(std::string_view s) override { add(s); }
   void on_double(std::string_view s) override { add(s); }
   void on_bool(std::string_view s) override { add(s); }
   void on_big_number(std::string_view s) override { add(s); }
   void on_verbatim_string(std::string_view s = {}) override { add(s); }
   void on_blob_string(std::string_view s = {}) override { add(s); }
   void select_array(int n) override { }
   void select_set(int n) override { }
   void select_map(int n) override { }
   void select_push(int n) override { }
   void on_streamed_string_part(std::string_view s = {}) override { add(s); }

   basic_array_type<T, Allocator> result;
};

template <
   class T,
   class Allocator = std::allocator<T>
   >
struct response_basic_map : response_base {
   void add(std::string_view s = {})
   {
      T r;
      from_string_view(s, r);
      result.emplace_back(std::move(r));
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

   basic_map_type<T, Allocator> result;
};

template <
   class T,
   class Allocator = std::allocator<T>
   >
struct response_basic_set : response_base {
   void add(std::string_view s = {})
   {
      T r;
      from_string_view(s, r);
      result.emplace_back(std::move(r));
   }

   void select_set(int n) override { }

   void on_simple_string(std::string_view s) override { add(s); }
   void on_number(std::string_view s) override { add(s); }
   void on_double(std::string_view s) override { add(s); }
   void on_bool(std::string_view s) override { add(s); }
   void on_big_number(std::string_view s) override { add(s); }
   void on_verbatim_string(std::string_view s = {}) override { add(s); }
   void on_blob_string(std::string_view s = {}) override { add(s); }

   basic_set_type<T, Allocator> result;
};

template <class T, std::size_t N>
struct response_static_array : response_base {
   int i_ = 0;
   void on_blob_string(std::string_view s) override
      { from_string_view(s, result[i_++]); }
   void select_array(int n) override { }

   std::array<T, N> result;
};

template <std::size_t N>
struct response_static_string : response_base {
   void add(std::string_view s)
     { result.assign(std::cbegin(s), std::cend(s));};
   void on_blob_string(std::string_view s) override
     { add(s); }
   void on_simple_string(std::string_view s) override
     { add(s); }

   boost::static_string<N> result;
};

template <
   class T,
   std::size_t N
   >
struct response_basic_static_map : response_base {
   int i_ = 0;

   void add(std::string_view s = {})
      { from_string_view(s, result.at(i_++)); }
   void on_blob_string(std::string_view s) override
      { add(s); }
   void on_number(std::string_view s) override
      { add(s); }

   void select_push(int n) override { }

   std::array<T, 2 * N> result;
};

} // resp
} // aedis

