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
void print(Iter begin, Iter end, char const* p)
{
  std::cout << p << ": ";
   for (; begin != end; ++begin)
     std::cout << *begin << " ";
   std::cout << std::endl;
}

template <class Range>
void print(Range const& v, char const* p = "")
{
   using std::cbegin;
   using std::cend;
   print(cbegin(v), cend(v), p);
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

class push {
private:
   void add(std::string_view s)
   {
      value.push_back({});
      from_string_view(s, value.back());
   }

public:
   void on_simple_string(std::string_view s) { add(s);}
   void on_number(std::string_view s) { add(s);}
   void on_double(std::string_view s) { add(s);}
   void on_bool(std::string_view s) { add(s);}
   void on_big_number(std::string_view s) { add(s);}
   void on_verbatim_string(std::string_view s) { add(s);}
   void on_blob_string(std::string_view s) { add(s);}

   std::vector<std::string> value;
};

template <class Push = push>
class response_base {
public:
   using push_type = Push;

private:
   bool is_null_ = false;
   error error_ = error::none;
   std::string error_msg_;
   bool is_push_ = false;
   push_type push_;

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
   virtual void select_push_impl(int n)
      { throw std::runtime_error("select_push_impl: Has not been overridden."); }

public:
   auto is_error() const noexcept {return error_ != error::none;}
   auto get_error() const noexcept {return error_;}
   auto const& error_message() const noexcept {return error_msg_;}
   auto is_push() const noexcept {return is_push_;}
   auto& push() noexcept {return push_;}
   auto const& push() const noexcept {return push_;}

   void pop()
      { aggregates_.pop_back(); }

   void on_simple_error(std::string_view s)
   {
      error_ = error::simple_error;
      error_msg_ = s;
   }

   void on_blob_error(std::string_view s = {})
   {
      error_ = error::blob_error;
      error_msg_ = s;
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
      is_push_ = true;
      aggregates_.push_back(aggregate_type::push);
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
      if (is_push_) {
	 push_.on_simple_string(s);
	 return;
      }

      on_simple_string_impl(s);
   }

   void on_number(std::string_view s)
   {
      if (is_push_) {
	 push_.on_number(s);
	 return;
      }

      on_number_impl(s);
   }

   void on_double(std::string_view s)
   {
      if (is_push_) {
	 push_.on_double(s);
	 return;
      }

      on_double_impl(s);
   }

   void on_bool(std::string_view s)
   {
      if (is_push_) {
	 push_.on_bool(s);
	 return;
      }

      on_bool_impl(s);
   }

   void on_big_number(std::string_view s)
   {
      if (is_push_) {
	 push_.on_big_number(s);
	 return;
      }

      on_big_number_impl(s);
   }

   void on_verbatim_string(std::string_view s = {})
   {
      if (is_push_) {
	 push_.on_verbatim_string(s);
	 return;
      }

      on_verbatim_string_impl(s);
   }

   void on_blob_string(std::string_view s = {})
   {
      if (is_push_) {
	 push_.on_blob_string(s);
	 return;
      }

      on_blob_string_impl(s);
   }

   virtual void on_streamed_string_part(std::string_view s = {})
      { throw std::runtime_error("on_streamed_string_part: Has not been overridden."); }

   virtual ~response_base() {}
};

template <class T, class Push = push>
class response_number : public response_base<Push> {
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
   class Allocator = std::allocator<CharT>,
   class Push = push>
class response_blob_string : public response_base<Push> {
private:
   void on_blob_string_impl(std::string_view s) override
      { from_string_view(s, result); }

public:
   std::basic_string<CharT, Traits, Allocator> result;
};

template<
   class CharT = char,
   class Traits = std::char_traits<CharT>,
   class Allocator = std::allocator<CharT>,
   class Push = push>
class response_simple_string : public response_base<Push> {
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
   class Allocator = std::allocator<CharT>,
   class Push = push>
class response_big_number : public response_base<Push> {
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
   class Allocator = std::allocator<CharT>,
   class Push = push>
class response_double : public response_base<Push> {
private:
   void on_double_impl(std::string_view s) override
      { from_string_view(s, result); }

public:
   std::basic_string<CharT, Traits, Allocator> result;
};

template <
   class T,
   class Allocator = std::allocator<T>,
   class Push = push>
class response_list : public response_base<Push> {
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
   class Allocator = std::allocator<CharT>,
   class Push = push>
class response_verbatim_string : public response_base<Push> {
private:
   void on_verbatim_string_impl(std::string_view s) override
      { from_string_view(s, result); }
public:
   std::basic_string<CharT, Traits, Allocator> result;
};

template<
   class CharT = char,
   class Traits = std::char_traits<CharT>,
   class Allocator = std::allocator<CharT>,
   class Push = push>
struct response_streamed_string : response_base<Push> {
   std::basic_string<CharT, Traits, Allocator> result;
   void on_streamed_string_part(std::string_view s) override
      { result += s; }
};

template<
   class Key,
   class Compare = std::less<Key>,
   class Allocator = std::allocator<Key>,
   class Push = push>
class response_set : public response_base<Push> {
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

template <class Push = push>
class response_bool : public response_base<Push> {
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
   class Allocator = std::allocator<Key>,
   class Push = push>
class response_unordered_set : public response_base<Push> {
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
   class Allocator = std::allocator<T>,
   class Push = push>
class response_array : public response_base<Push> {
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

template <
   class T,
   std::size_t N,
   class Push = push>
class response_static_array : public response_base<Push> {
private:
   int i = 0;
   void on_blob_string_impl(std::string_view s) override
      { from_string_view(s, result[i++]); }

public:
   std::array<T, N> result;
};

template <
   class T,
   std::size_t N,
   class Push = push>
class response_static_flat_map : public response_base<Push> {
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

} // resp
} // aedis
