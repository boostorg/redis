/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <set>
#include <list>
#include <array>
#include <vector>
#include <string>
#include <ostream>
#include <numeric>
#include <type_traits>
#include <string_view>
#include <charconv>
#include <iomanip>

#include <boost/static_string/static_string.hpp>

#include "type.hpp"
#include "command.hpp"

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

// The interface required from from the parser.
struct response_ignore {
   void pop() {}
   void select_array(int n) {}
   void select_push(int n) {}
   void select_set(int n) {}
   void select_map(int n) {}
   void select_attribute(int n) {}

   void on_simple_string(std::string_view s) {}
   void on_simple_error(std::string_view s) {}
   void on_number(std::string_view s) {}
   void on_double(std::string_view s) {}
   void on_bool(std::string_view s) {}
   void on_big_number(std::string_view s) {}
   void on_null() {}
   void on_blob_error(std::string_view s = {}) {}
   void on_verbatim_string(std::string_view s = {}) {}
   void on_blob_string(std::string_view s = {}) {}
   void on_streamed_string_part(std::string_view s = {}) {}
   ~response_ignore() {}
};

// A base class for flat responses which means response with no
// embedded types in themselves. For exaple, a transaction with an
// lrange in it will produce a response that is an array with an
// array. That is not suitable for this class.
class response_base {
protected:
   virtual void on_simple_string_impl(std::string_view s)
      { throw std::runtime_error("on_simple_string_impl: Has not been overridden."); }
   virtual void on_simple_error_impl(std::string_view s)
      { throw std::runtime_error("on_simple_error_impl: Has not been overridden."); }
   virtual void on_number_impl(std::string_view s)
      { throw std::runtime_error("on_number_impl: Has not been overridden."); }
   virtual void on_double_impl(std::string_view s)
      { throw std::runtime_error("on_double_impl: Has not been overridden."); }
   virtual void on_null_impl()
      { throw std::runtime_error("on_null_impl: Has not been overridden."); }
   virtual void on_bool_impl(std::string_view s)
      { throw std::runtime_error("on_bool_impl: Has not been overridden."); }
   virtual void on_big_number_impl(std::string_view s)
      { throw std::runtime_error("on_big_number_impl: Has not been overridden."); }
   virtual void on_verbatim_string_impl(std::string_view s = {})
      { throw std::runtime_error("on_verbatim_string_impl: Has not been overridden."); }
   virtual void on_blob_string_impl(std::string_view s = {})
      { throw std::runtime_error("on_blob_string_impl: Has not been overridden."); }
   virtual void on_blob_error_impl(std::string_view s = {})
      { throw std::runtime_error("on_blob_error_impl: Has not been overridden."); }
   virtual void on_streamed_string_part_impl(std::string_view s = {})
      { throw std::runtime_error("on_streamed_string_part: Has not been overridden."); }
   virtual void select_array_impl(int n)
      { throw std::runtime_error("select_array_impl: Has not been overridden."); }
   virtual void select_set_impl(int n)
      { throw std::runtime_error("select_set_impl: Has not been overridden."); }
   virtual void select_map_impl(int n)
      { throw std::runtime_error("select_map_impl: Has not been overridden."); }
   virtual void select_push_impl(int n)
      { throw std::runtime_error("select_push_impl: Has not been overridden."); }
   virtual void select_attribute_impl(int n)
      { throw std::runtime_error("select_attribute_impl: Has not been overridden."); }

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

// This response type is able to deal with recursive redis responses as in a
// transaction for example.
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
      assert(!std::empty(result));
      if (std::ssize(result.back().value) == result.back().expected_size) {
	 result.emplace_back(depth_, t, 1, std::vector<std::string>{std::string{s}});
      } else {
	 result.back().value.push_back(std::string{s});
      }
   }

   void select_array_impl(int n) override {add_aggregate(n, type::array);}
   void select_push_impl(int n) override {add_aggregate(n, type::push);}
   void select_set_impl(int n) override {add_aggregate(n, type::set);}
   void select_map_impl(int n) override {add_aggregate(n, type::map);}
   void select_attribute_impl(int n) override {add_aggregate(n, type::attribute);}

   void on_simple_string_impl(std::string_view s) override { add(s, type::simple_string); }
   void on_simple_error_impl(std::string_view s) override { add(s, type::simple_error); }
   void on_number_impl(std::string_view s) override {add(s, type::number);}
   void on_double_impl(std::string_view s) override {add(s, type::double_type);}
   void on_bool_impl(std::string_view s) override {add(s, type::boolean);}
   void on_big_number_impl(std::string_view s) override {add(s, type::big_number);}
   void on_null_impl() override {add({}, type::null);}
   void on_blob_error_impl(std::string_view s = {}) override {add(s, type::blob_error);}
   void on_verbatim_string_impl(std::string_view s = {}) override {add(s, type::verbatim_string);}
   void on_blob_string_impl(std::string_view s = {}) override {add(s, type::blob_string);}
   void on_streamed_string_part_impl(std::string_view s = {}) override {add(s, type::streamed_string_part);}

public:
   void clear() { result.clear(); depth_ = 0;}
   auto size() const { return result.size(); }
   void pop() override { --depth_; }
};

template <class T>
class response_number : public response_base {
   static_assert(std::is_integral<T>::value);
private:
   void on_number_impl(std::string_view s) override
      { from_string_view(s, result); }

public:
   T result;
};

template<
   class CharT = char,
   class Traits = std::char_traits<CharT>,
   class Allocator = std::allocator<CharT>>
class response_blob_string : public response_base {
private:
   void add(std::string_view s)
      { from_string_view(s, result); }

   void on_blob_string_impl(std::string_view s) override
      { add(s); }
   void on_blob_error_impl(std::string_view s) override
      { add(s); }
   void on_simple_string_impl(std::string_view s) override
      { add(s); }
   void on_simple_error_impl(std::string_view s) override
      { add(s); }
public:
   std::basic_string<CharT, Traits, Allocator> result;
};

template<
   class CharT = char,
   class Traits = std::char_traits<CharT>,
   class Allocator = std::allocator<CharT>
   >
class response_simple_string : public response_base {
private:
   void add(std::string_view s)
      { from_string_view(s, result); }

   void on_simple_string_impl(std::string_view s) override
      { add(s); }
   void on_simple_error_impl(std::string_view s) override
      { add(s); }

public:
   std::basic_string<CharT, Traits, Allocator> result;
};

// Big number use strings at the moment as the underlying storage.
template <
   class CharT = char,
   class Traits = std::char_traits<CharT>,
   class Allocator = std::allocator<CharT>
   >
class response_big_number : public response_base {
private:
   void on_big_number_impl(std::string_view s) override
      { from_string_view(s, result); }

public:
   std::basic_string<CharT, Traits, Allocator> result;
};

// TODO: Use a double instead of string.
template <
   class CharT = char,
   class Traits = std::char_traits<CharT>,
   class Allocator = std::allocator<CharT>
   >
class response_double : public response_base {
private:
   void on_double_impl(std::string_view s) override
      { from_string_view(s, result); }

public:
   std::basic_string<CharT, Traits, Allocator> result;
};

template <
   class T,
   class Allocator = std::allocator<T>>
class response_list : public response_base {
private:
   void on_blob_string_impl(std::string_view s) override
   {
      T r;
      from_string_view(s, r);
      result.push_back(std::move(r));
   }

   void select_array_impl(int n) override { }

public:
   std::list<T, Allocator> result;
};

template<
   class CharT = char,
   class Traits = std::char_traits<CharT>,
   class Allocator = std::allocator<CharT>
   >
class response_verbatim_string : public response_base {
private:
   void on_verbatim_string_impl(std::string_view s) override
      { from_string_view(s, result); }
public:
   std::basic_string<CharT, Traits, Allocator> result;
};

template<
   class CharT = char,
   class Traits = std::char_traits<CharT>,
   class Allocator = std::allocator<CharT>
   >
class response_streamed_string : public response_base {
private:
   void on_streamed_string_part_impl(std::string_view s) override
      { result += s; }
public:
   std::basic_string<CharT, Traits, Allocator> result;
};

template <
   class Key,
   class Compare = std::less<Key>,
   class Allocator = std::allocator<Key>
   >
class response_set : public response_base {
private:
   void add(std::string_view s)
   {
      Key r;
      from_string_view(s, r);
      result.insert(std::end(result), std::move(r));
   }

   void on_simple_string_impl(std::string_view s) override { add(s); }
   void on_blob_string_impl(std::string_view s) override { add(s); }
   void select_set_impl(int n) override { }

public:
   std::set<Key, Compare, Allocator> result;
};

class response_bool : public response_base {
private:
   void on_bool_impl(std::string_view s) override
   {
      if (std::ssize(s) != 1) {
	 // We can't hadle an error in redis.
	 throw std::runtime_error("Bool has wrong size");
      }

      result = s[0] == 't';
   }

public:
   bool result;
};

template<
   class Key,
   class Compare = std::less<Key>,
   class Allocator = std::allocator<Key>
   >
class response_unordered_set : public response_base {
private:
   void on_blob_string_impl(std::string_view s) override
   {
      Key r;
      from_string_view(s, r);
      result.insert(std::end(result), std::move(r));
   }

   void select_array_impl(int n) override { }
   void select_set_impl(int n) override { }

public:
   std::set<Key, Compare, Allocator> result;
};

template <
   class T,
   class Allocator = std::allocator<T>
   >
class response_array : public response_base {
private:
   void add(std::string_view s = {})
   {
      T r;
      from_string_view(s, r);
      result.emplace_back(std::move(r));
   }

   void on_simple_string_impl(std::string_view s) override { add(s); }
   void on_number_impl(std::string_view s) override { add(s); }
   void on_double_impl(std::string_view s) override { add(s); }
   void on_bool_impl(std::string_view s) override { add(s); }
   void on_big_number_impl(std::string_view s) override { add(s); }
   void on_verbatim_string_impl(std::string_view s = {}) override { add(s); }
   void on_blob_string_impl(std::string_view s = {}) override { add(s); }
   void select_array_impl(int n) override { }
   void select_set_impl(int n) override { }
   void select_map_impl(int n) override { }
   void select_push_impl(int n) override { }
   void on_streamed_string_part_impl(std::string_view s = {}) override { add(s); }

public:
   std::vector<T, Allocator> result;
};

template <class T, class Allocator = std::allocator<T>>
using response_flat_map = response_array<T, Allocator>;

template <class T, class Allocator = std::allocator<T>>
using response_flat_set = response_array<T, Allocator>;

template <class T, std::size_t N>
class response_static_array : public response_base {
private:
   int i = 0;
   void on_blob_string_impl(std::string_view s) override
      { from_string_view(s, result[i++]); }

public:
   std::array<T, N> result;
};

template <class T, std::size_t N>
class response_static_string : public response_base {
private:
   void add(std::string_view s)
     { result.assign(std::cbegin(s), std::cend(s));};
   void on_blob_string_impl(std::string_view s) override
     { add(s); }
   void on_simple_string_impl(std::string_view s) override
     { add(s); }

public:
   boost::static_string<N> result;
};

template <
   class T,
   std::size_t N
   >
class response_static_flat_map : public response_base {
private:
   int i = 0;

   void add(std::string_view s = {})
      { from_string_view(s, result.at(i++)); }
   void on_blob_string_impl(std::string_view s) override
      { add(s); }
   void on_number_impl(std::string_view s) override
      { add(s); }

   void select_push_impl(int n) override { }

public:
   std::array<T, 2 * N> result;
};

template <class Event>
struct response_id {
   command cmd;
   type t;
   Event event;
};

template <class Event>
class responses {
private:
   response_tree tree_;
   response_array<std::string> array_;

public:
   // When the id is from a transaction the type of the message is not
   // specified.
   response_base* get(response_id<Event> id)
   {
      if (id.cmd == command::exec)
        return &tree_;

      return &array_;
   }

   void clear_transaction() { tree_.clear(); }
   void clear() { array_.result.clear(); }
};

} // resp
} // aedis

template <class Event>
std::ostream&
operator<<(std::ostream& os, aedis::resp::response_id<Event> const& id)
{
   os
      << std::left << std::setw(15) << aedis::resp::to_string(id.cmd)
      << std::left << std::setw(20) << aedis::resp::to_string(id.t)
      << std::left << std::setw(20) << id.event
   ;
   return os;
}
