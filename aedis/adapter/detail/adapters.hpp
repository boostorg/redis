/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <set>
#include <unordered_set>
#include <forward_list>
#include <optional>
#include <system_error>
#include <map>
#include <unordered_map>
#include <list>
#include <deque>
#include <vector>
#include <charconv>
#include <array>

#include <aedis/resp3/type.hpp>
#include <aedis/generic/serializer.hpp>
#include <aedis/resp3/node.hpp>
#include <aedis/adapter/error.hpp>

namespace aedis {
namespace adapter {
namespace detail {

// Serialization.

template <class T>
typename std::enable_if<std::is_integral<T>::value, void>::type
from_string(
   T& i,
   char const* value,
   std::size_t data_size,
   boost::system::error_code& ec)
{
   auto const res = std::from_chars(value, value + data_size, i);
   if (res.ec != std::errc())
      ec = std::make_error_code(res.ec);
}

void from_string(
   bool& t,
   char const* value,
   std::size_t size,
   boost::system::error_code& ec)
{
   t = *value == 't';
}

template <class CharT, class Traits, class Allocator>
void
from_string(
   std::basic_string<CharT, Traits, Allocator>& s,
   char const* value,
   std::size_t data_size,
   boost::system::error_code&)
{
  s.append(value, data_size);
}

//================================================

void set_on_resp3_error(resp3::type t, boost::system::error_code& ec)
{
   switch (t) {
      case resp3::type::simple_error: ec = adapter::error::simple_error; return;
      case resp3::type::blob_error: ec = adapter::error::blob_error; return;
      case resp3::type::null: ec = adapter::error::null; return;
      default: return;
   }
}

template <class Result>
class general_aggregate {
private:
   Result* result_;

public:
   general_aggregate(Result* c = nullptr): result_(c) {}
   void operator()( resp3::type t, std::size_t aggregate_size, std::size_t depth, char const* value, std::size_t size, boost::system::error_code&)
      { result_->push_back({t, aggregate_size, depth, std::string{value, size}}); }
};

template <class Node>
class general_simple {
private:
   Node* result_;

public:
   general_simple(Node* t = nullptr) : result_(t) {}

   void operator()( resp3::type t, std::size_t aggregate_size, std::size_t depth, char const* value, std::size_t data_size, boost::system::error_code&)
   {
     result_->data_type = t;
     result_->aggregate_size = aggregate_size;
     result_->depth = depth;
     result_->value.assign(value, data_size);
   }
};

template <class Result>
class simple_impl {
public:
   void on_value_available(Result&) {}
   void operator()(Result& result, resp3::type t, std::size_t aggregate_size, std::size_t depth, char const* value, std::size_t size, boost::system::error_code& ec)
   {
      set_on_resp3_error(t, ec);
      if (ec)
         return;

      if (is_aggregate(t)) {
         ec = adapter::error::expects_simple_type;
         return;
      }

      from_string(result, value, size, ec);
   }
};

template <class Result>
class set_impl {
private:
   typename Result::iterator hint_;

public:
   void on_value_available(Result& result)
      { hint_ = std::end(result); }

   void
   operator()(
       Result& result,
       resp3::type t,
       std::size_t aggregate_size,
       std::size_t depth,
       char const* value,
       std::size_t data_size,
       boost::system::error_code& ec)
   {
      set_on_resp3_error(t, ec);
      if (ec)
         return;

      if (is_aggregate(t)) {
         if (t != resp3::type::set)
            ec = error::expects_set_aggregate;
         return;
      }

      assert(aggregate_size == 1);

      if (depth < 1) {
	 ec = adapter::error::expects_set_aggregate;
	 return;
      }

      typename Result::key_type obj;
      from_string(obj, value, data_size, ec);
      hint_ = result.insert(hint_, std::move(obj));
   }
};

template <class Result>
class map_impl {
private:
   typename Result::iterator current_;
   bool on_key_ = true;

public:
   void on_value_available(Result& result)
      { current_ = std::end(result); }

   void
   operator()(
       Result& result,
       resp3::type t,
       std::size_t aggregate_size,
       std::size_t depth,
       char const* value,
       std::size_t data_size,
       boost::system::error_code& ec)
   {
      set_on_resp3_error(t, ec);
      if (ec)
         return;

      if (is_aggregate(t)) {
         if (element_multiplicity(t) != 2)
           ec = error::expects_map_like_aggregate;
         return;
      }

      assert(aggregate_size == 1);

      if (depth < 1) {
	 ec = adapter::error::expects_map_like_aggregate;
	 return;
      }

      if (on_key_) {
         typename Result::key_type obj;
         from_string(obj, value, data_size, ec);
         current_ = result.insert(current_, {std::move(obj), {}});
      } else {
         typename Result::mapped_type obj;
         from_string(obj, value, data_size, ec);
         current_->second = std::move(obj);
      }

      on_key_ = !on_key_;
   }
};

template <class Result>
class vector_impl {
public:
   void on_value_available(Result& ) { }

   void
   operator()(
       Result& result,
       resp3::type t,
       std::size_t aggregate_size,
       std::size_t depth,
       char const* value,
       std::size_t data_size,
       boost::system::error_code& ec)
   {
      set_on_resp3_error(t, ec);
      if (ec)
         return;

      if (is_aggregate(t)) {
         auto const m = element_multiplicity(t);
         result.reserve(result.size() + m * aggregate_size);
      } else {
         result.push_back({});
         from_string(result.back(), value, data_size, ec);
      }
   }
};

template <class Result>
class array_impl {
private:
   int i_ = -1;

public:
   void on_value_available(Result& ) { }

   void
   operator()(
       Result& result,
       resp3::type t,
       std::size_t aggregate_size,
       std::size_t depth,
       char const* value,
       std::size_t data_size,
       boost::system::error_code& ec)
   {
      set_on_resp3_error(t, ec);
      if (ec)
         return;

      if (is_aggregate(t)) {
	 if (i_ != -1) {
            ec = adapter::error::nested_aggregate_unsupported;
            return;
         }

         if (result.size() != aggregate_size * element_multiplicity(t)) {
           ec = error::incompatible_size;
           return;
         }
      } else {
         if (i_ == -1) {
            ec = adapter::error::expects_aggregate;
            return;
         }

         assert(aggregate_size == 1);
         from_string(result.at(i_), value, data_size, ec);
      }

      ++i_;
   }
};

template <class Result>
struct list_impl {

   void on_value_available(Result& ) { }

   void
   operator()(
       Result& result,
       resp3::type t,
       std::size_t aggregate_size,
       std::size_t depth,
       char const* value,
       std::size_t data_size,
       boost::system::error_code& ec)
   {
      set_on_resp3_error(t, ec);
      if (ec)
         return;

      if (!is_aggregate(t)) {
        assert(aggregate_size == 1);
        if (depth < 1) {
           ec = adapter::error::expects_aggregate;
           return;
        }

        result.push_back({});
        from_string(result.back(), value, data_size, ec);
      }
   }
};

//---------------------------------------------------

template <class T>
struct impl_map { using type = simple_impl<T>; };

template <class Key, class Compare, class Allocator>
struct impl_map<std::set<Key, Compare, Allocator>> { using type = set_impl<std::set<Key, Compare, Allocator>>; };

template <class Key, class Compare, class Allocator>
struct impl_map<std::multiset<Key, Compare, Allocator>> { using type = set_impl<std::multiset<Key, Compare, Allocator>>; };

template <class Key, class Hash, class KeyEqual, class Allocator>
struct impl_map<std::unordered_set<Key, Hash, KeyEqual, Allocator>> { using type = set_impl<std::unordered_set<Key, Hash, KeyEqual, Allocator>>; };

template <class Key, class Hash, class KeyEqual, class Allocator>
struct impl_map<std::unordered_multiset<Key, Hash, KeyEqual, Allocator>> { using type = set_impl<std::unordered_multiset<Key, Hash, KeyEqual, Allocator>>; };

template <class Key, class T, class Compare, class Allocator>
struct impl_map<std::map<Key, T, Compare, Allocator>> { using type = map_impl<std::map<Key, T, Compare, Allocator>>; };

template <class Key, class T, class Compare, class Allocator>
struct impl_map<std::multimap<Key, T, Compare, Allocator>> { using type = map_impl<std::multimap<Key, T, Compare, Allocator>>; };

template <class Key, class Hash, class KeyEqual, class Allocator>
struct impl_map<std::unordered_map<Key, Hash, KeyEqual, Allocator>> { using type = map_impl<std::unordered_map<Key, Hash, KeyEqual, Allocator>>; };

template <class Key, class Hash, class KeyEqual, class Allocator>
struct impl_map<std::unordered_multimap<Key, Hash, KeyEqual, Allocator>> { using type = map_impl<std::unordered_multimap<Key, Hash, KeyEqual, Allocator>>; };

template <class T, class Allocator>
struct impl_map<std::vector<T, Allocator>> { using type = vector_impl<std::vector<T, Allocator>>; };

template <class T, std::size_t N>
struct impl_map<std::array<T, N>> { using type = array_impl<std::array<T, N>>; };

template <class T, class Allocator>
struct impl_map<std::list<T, Allocator>> { using type = list_impl<std::list<T, Allocator>>; };

template <class T, class Allocator>
struct impl_map<std::deque<T, Allocator>> { using type = list_impl<std::deque<T, Allocator>>; };

//---------------------------------------------------

template <class Result>
class wrapper {
private:
   Result* result_;
   typename impl_map<Result>::type impl_;

public:
   wrapper(Result* t = nullptr) : result_(t)
      { impl_.on_value_available(*result_); }

   void operator()( resp3::type t, std::size_t aggregate_size, std::size_t depth, char const* value, std::size_t size, boost::system::error_code& ec)
   {
      assert(result_);
      impl_(*result_, t, aggregate_size, depth, value, size, ec);
   }
};

template <class T>
class wrapper<std::optional<T>> {
private:
   std::optional<T>* result_;
   typename impl_map<T>::type impl_;

public:
   wrapper(std::optional<T>* o = nullptr) : result_(o), impl_{} {}

   void operator()( resp3::type t, std::size_t aggregate_size, std::size_t depth, char const* value, std::size_t size, boost::system::error_code& ec)
   {
      if (t == resp3::type::null)
         return;

      if (!result_->has_value()) {
        *result_ = T{};
        impl_.on_value_available(result_->value());
      }

      impl_(result_->value(), t, aggregate_size, depth, value, size, ec);
   }
};

} // detail
} // adapter
} // aedis
