/* Copyright (c) 2019 - 2020 Marcelo Zimbres Silva (mzimbres at gmail dot com)
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
#include <numeric>
#include <type_traits>
#include <string_view>
#include <charconv>

template <class Iter>
void print(Iter begin, Iter end)
{
   for (; begin != end; ++begin)
     std::cout << *begin << " ";
   std::cout << std::endl;
}

template <class Range>
void print(Range const& v)
{
   using std::cbegin;
   using std::cend;
   print(cbegin(v), cend(v));
}

namespace aedis { namespace resp {

template <class T>
std::enable_if<std::is_integral<T>::value, void>::type
from_string_view(std::string_view s, T& n)
{
   auto r = std::from_chars(s.data(), s.data() + s.size(), n);
   if (r.ec == std::errc::invalid_argument)
      throw std::runtime_error("from_chars: Unable to convert");
}

void from_string_view(std::string_view s, std::string& r)
   { r = s; }

// The interface required from all response types.
struct response_noop {
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
   ~response_noop() {}
};

using response = response_noop;

enum class error
{ simple_error
, blob_error
, none
};

enum class aggregate_type
{ attribute
, push
, array
, set
, map
, none
};

class response_base {
private:
   error err_ = error::none;
   bool is_null_ = false;
   std::string err_msg_;

   // The first element is the sentinel.
   std::vector<aggregate_type> aggregates_ {aggregate_type::attribute};

protected:
   virtual void on_simple_string_impl(std::string_view s)
      { throw std::runtime_error("on_simple_string_impl: Has not been overridden."); }
   virtual void on_number_impl(std::string_view s)
      { throw std::runtime_error("on_number_impl: Has not been overridden."); }
   virtual void on_double_impl(std::string_view s)
      { throw std::runtime_error("on_double_impl: Has not been overridden."); }
   virtual void on_bool_impl(std::string_view s)
      { throw std::runtime_error("on_bool_impl: Has not been overridden."); }
   virtual void on_big_number_impl(std::string_view s)
      { throw std::runtime_error("on_big_number_impl: Has not been overridden."); }
   virtual void on_verbatim_string_impl(std::string_view s = {})
      { throw std::runtime_error("on_verbatim_string_impl: Has not been overridden."); }
   virtual void on_blob_string_impl(std::string_view s = {})
      { throw std::runtime_error("on_blob_string_impl: Has not been overridden."); }
   virtual void select_array_impl(int n)
      { throw std::runtime_error("select_array_impl: Has not been overridden."); }
   virtual void select_set_impl(int n)
      { throw std::runtime_error("select_set_impl: Has not been overridden."); }
   virtual void select_map_impl(int n)
      { throw std::runtime_error("select_map_impl: Has not been overridden."); }

public:
   auto get_error() const noexcept {return err_;}
   auto const& message() const noexcept {return err_msg_;}

   void pop()
      { aggregates_.pop_back(); }

   void on_simple_error(std::string_view s)
   {
      err_ = error::simple_error;
      err_msg_ = s;
   }

   void on_blob_error(std::string_view s = {})
   {
      err_ = error::blob_error;
      err_msg_ = s;
   }

   void on_null() {is_null_ = true; }

   auto is_attribute() const noexcept
   {
      auto i = std::ssize(aggregates_) - 1;
      while (aggregates_[i] != aggregate_type::attribute)
	 --i;

      return i != 0;
   }

   void select_attribute(int n)
   {
      aggregates_.push_back(aggregate_type::attribute);
      throw std::runtime_error("select_attribute: Has not been overridden.");
   }

   void select_push(int n)
   {
      aggregates_.push_back(aggregate_type::push);
      throw std::runtime_error("select_push: Has not been overridden.");
   }

   void select_array(int n)
   {
      aggregates_.push_back(aggregate_type::array);
   }

   void select_set(int n)
   {
      aggregates_.push_back(aggregate_type::set);
   }

   void select_map(int n)
   {
      aggregates_.push_back(aggregate_type::map);
   }

   void on_simple_string(std::string_view s)
   {
      on_simple_string_impl(s);
   }

   void on_number(std::string_view s)
   {
      on_number_impl(s);
   }

   void on_double(std::string_view s)
   {
      on_double_impl(s);
   }

   void on_bool(std::string_view s)
   {
      on_bool_impl(s);
   }

   void on_big_number(std::string_view s)
   {
      on_big_number_impl(s);
   }

   void on_verbatim_string(std::string_view s = {})
   {
      on_verbatim_string_impl(s);
   }

   void on_blob_string(std::string_view s = {})
   {
      on_blob_string_impl(s);
   }

   virtual void on_streamed_string_part(std::string_view s = {}) { throw std::runtime_error("on_streamed_string_part: Has not been overridden."); }
   virtual ~response_base() {}
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
   void on_blob_string_impl(std::string_view s) override
      { from_string_view(s, result); }

public:
   std::basic_string<CharT, Traits, Allocator> result;
};

template<
   class CharT = char,
   class Traits = std::char_traits<CharT>,
   class Allocator = std::allocator<CharT>>
class response_simple_string : public response_base {
private:
   void on_simple_string_impl(std::string_view s) override
      { from_string_view(s, result); }

public:
   std::basic_string<CharT, Traits, Allocator> result;
};

// Big number use strings at the moment as the underlying storage.
template<
   class CharT = char,
   class Traits = std::char_traits<CharT>,
   class Allocator = std::allocator<CharT>>
class response_big_number : public response_base {
private:
   void on_big_number_impl(std::string_view s) override
      { from_string_view(s, result); }

public:
   std::basic_string<CharT, Traits, Allocator> result;
};

// TODO: Use a double instead of string.
template<
   class CharT = char,
   class Traits = std::char_traits<CharT>,
   class Allocator = std::allocator<CharT>>
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
   class Allocator = std::allocator<CharT>>
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
   class Allocator = std::allocator<CharT>>
struct response_streamed_string : response_base {
   std::basic_string<CharT, Traits, Allocator> result;
   void on_streamed_string_part(std::string_view s) override
      { result += s; }
};

template<
   class Key,
   class Compare = std::less<Key>,
   class Allocator = std::allocator<Key>>
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
   class Allocator = std::allocator<Key>>
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

template <class T, class Allocator = std::allocator<T>>
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

public:
   std::vector<T, Allocator> result;

   void clear() { result.clear(); }
   auto size() const noexcept { return std::size(result); }

   void on_streamed_string_part(std::string_view s = {}) override { add(s); }
};

template <class T, class Allocator = std::allocator<T>>
using response_flat_map = response_array<T, Allocator>;

template <class T, class Allocator = std::allocator<T>>
using response_flat_set = response_array<T, Allocator>;

class response_publish : public response_base {
private:
   int i = 0;
   void on_blob_string_impl(std::string_view s) override
      { from_string_view(s, result[i++]); }

public:

   std::array<std::string, 3> result;
};

} // resp
} // aedis
