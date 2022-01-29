/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <set>
#include <optional>
#include <system_error>
#include <map>
#include <list>
#include <deque>
#include <vector>
#include <charconv>

#include <aedis/resp3/type.hpp>
#include <aedis/resp3/node.hpp>
#include <aedis/resp3/serializer.hpp>
#include <aedis/resp3/adapter/error.hpp>

namespace aedis {
namespace resp3 {
namespace adapter {
namespace detail {

template <class T>
typename std::enable_if<std::is_integral<T>::value, void>::type
from_string(
   T& i,
   char const* data,
   std::size_t data_size,
   std::error_code& ec)
{
   auto const res = std::from_chars(data, data + data_size, i);
   if (res.ec != std::errc())
      ec = std::make_error_code(res.ec);
}

template <class CharT, class Traits, class Allocator>
void
from_string(
   std::basic_string<CharT, Traits, Allocator>& s,
   char const* data,
   std::size_t data_size,
   std::error_code&)
{
  s.assign(data, data_size);
}

void set_on_resp3_error(type t, std::error_code& ec)
{
   switch (t) {
      case type::simple_error: ec = adapter::error::simple_error; return;
      case type::blob_error: ec = adapter::error::blob_error; return;
      case type::null: ec = adapter::error::null; return;
      default: return;
   }
}

// For optional responses.
void set_on_resp3_error2(type t, std::error_code& ec)
{
   switch (t) {
      case type::simple_error: ec = adapter::error::simple_error; return;
      case type::blob_error: ec = adapter::error::blob_error; return;
      default: return;
   }
}

// Adapter that ignores responses.
struct ignore {
   void
   operator()(
      type, std::size_t, std::size_t, char const*, std::size_t,
      std::error_code&) { }
};

template <class Container>
class general {
private:
   Container* result_;

public:
   general(Container* c = nullptr): result_(c) {}

   /** @brief Function called by the parser when new data has been processed.
    *  
    *  Users who what to customize their response types are required to derive
    *  from this class and override this function, see examples.
    *
    *  \param t The RESP3 type of the data.
    *
    *  \param n When t is an aggregate data type this will contain its size
    *     (see also element_multiplicity) for simple data types this is always 1.
    *
    *  \param depth The element depth in the tree.
    *
    *  \param data A pointer to the data.
    *
    *  \param size The size of data.
    */
   void
   operator()(
      type t,
      std::size_t aggregate_size,
      std::size_t depth,
      char const* data,
      std::size_t size,
      std::error_code&)
      {
	 result_->emplace_back(t, aggregate_size, depth, std::string{data, size});
      }
};

template <class Node>
class adapter_node {
private:
   Node* result_;

public:
   adapter_node(Node* t = nullptr) : result_(t) {}

   void
   operator()(
      type t,
      std::size_t aggregate_size,
      std::size_t depth,
      char const* data,
      std::size_t data_size,
      std::error_code&)
   {
     result_->data_type = t;
     result_->aggregate_size = aggregate_size;
     result_->depth = depth;
     result_->data.assign(data, data_size);
   }
};

// Adapter for RESP3 simple data types.
template <class T>
class simple {
private:
   T* result_;

public:
   simple(T* t = nullptr) : result_(t) {}

   void
   operator()(
      type t,
      std::size_t aggregate_size,
      std::size_t depth,
      char const* data,
      std::size_t data_size,
      std::error_code& ec)
   {
      set_on_resp3_error(t, ec);

      if (is_aggregate(t)) {
	 ec = adapter::error::expects_simple_type;
	 return;
      }

      assert(aggregate_size == 1);
      from_string(*result_, data, data_size, ec);
   }
};

template <class T>
class simple_optional {
private:
  std::optional<T>* result_;

public:
   simple_optional(std::optional<T>* o = nullptr) : result_(o) {}

   void
   operator()(
      type t,
      std::size_t aggregate_size,
      std::size_t depth,
      char const* data,
      std::size_t data_size,
      std::error_code& ec)
   {
      set_on_resp3_error2(t, ec);

      if (is_aggregate(t)) {
	 ec = adapter::error::expects_simple_type;
	 return;
      }

      assert(aggregate_size == 1);

      if (depth != 0) {
	 ec = adapter::error::nested_unsupported;
	 return;
      }

      if (t == type::null)
         return;

      if (!result_->has_value())
        *result_ = T{};

      from_string(result_->value(), data, data_size, ec);
   }
};

/* A std::vector adapter.
 */
template <class Container>
class vector {
private:
   int i_ = -1;
   Container* result_;

public:
   vector(Container* v = nullptr) : result_{v} {}

   void
   operator()(type t,
       std::size_t aggregate_size,
       std::size_t depth,
       char const* data,
       std::size_t data_size,
       std::error_code& ec)
   {
      set_on_resp3_error(t, ec);

      if (is_aggregate(t)) {
	 if (i_ != -1) {
	    ec = adapter::error::nested_unsupported;
	    return;
	 }

         auto const m = element_multiplicity(t);
         result_->resize(m * aggregate_size);
         ++i_;
      } else {
	 assert(aggregate_size == 1);

         from_string(result_->at(i_), data, data_size, ec);
         ++i_;
      }
   }
};

template <class Container>
class list {
private:
   Container* result_;

public:
   list(Container* ref = nullptr): result_(ref) {}

   void
   operator()(type t,
       std::size_t aggregate_size,
       std::size_t depth,
       char const* data,
       std::size_t data_size,
       std::error_code& ec)
   {
      set_on_resp3_error(t, ec);

      if (is_aggregate(t)) {
	 if (depth != 0) {
	    ec = adapter::error::nested_unsupported;
	    return;
	 }
         return;
      }

      assert(aggregate_size == 1);

      if (depth != 1) {
	 ec = adapter::error::nested_unsupported;
	 return;
      }

      result_->push_back({});
      from_string(result_->back(), data, data_size, ec);
   }
};

template <class Container>
class set {
private:
   Container* result_;
   typename Container::iterator hint_;

public:
   set(Container* c = nullptr)
   : result_(c)
   , hint_(std::end(*c))
   {}

   void
   operator()(type t,
       std::size_t aggregate_size,
       std::size_t depth,
       char const* data,
       std::size_t data_size,
       std::error_code& ec)
   {
      set_on_resp3_error(t, ec);

      if (t == type::set) {
        assert(depth == 0);
        return;
      }

      assert(!is_aggregate(t));

      assert(depth == 1);
      assert(aggregate_size == 1);

      typename Container::key_type obj;
      from_string(obj, data, data_size, ec);
      if (hint_ == std::end(*result_)) {
         auto const ret = result_->insert(std::move(obj));
         hint_ = ret.first;
      } else {
         hint_ = result_->insert(hint_, std::move(obj));
      }
   }
};

template <class Container>
class map {
private:
   Container* result_;
   typename Container::iterator current_;
   bool on_key_ = true;

public:
   map(Container* c = nullptr)
   : result_(c)
   , current_(std::end(*c))
   {}

   void
   operator()(type t,
       std::size_t aggregate_size,
       std::size_t depth,
       char const* data,
       std::size_t data_size,
       std::error_code& ec)
   {
      set_on_resp3_error(t, ec);

      if (t == type::map) {
        assert(depth == 0);
        return;
      }

      assert(!is_aggregate(t));
      assert(depth == 1);
      assert(aggregate_size == 1);

      if (on_key_) {
         typename Container::key_type obj;
         from_string(obj, data, data_size, ec);
         current_ = result_->insert(current_, {std::move(obj), {}});
      } else {
         typename Container::mapped_type obj;
         from_string(obj, data, data_size, ec);
         current_->second = std::move(obj);
      }

      on_key_ = !on_key_;
   }
};

} // detail
} // adapter
} // resp3
} // aedis
