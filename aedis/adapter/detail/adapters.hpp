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
#include <aedis/resp3/serializer.hpp>
#include <aedis/adapter/node.hpp>
#include <aedis/adapter/error.hpp>

namespace aedis {
namespace adapter {
namespace detail {

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

template <class CharT, class Traits, class Allocator>
void
from_string(
   std::basic_string<CharT, Traits, Allocator>& s,
   char const* value,
   std::size_t data_size,
   boost::system::error_code&)
{
  s.assign(value, data_size);
}

void set_on_resp3_error(resp3::type t, boost::system::error_code& ec)
{
   switch (t) {
      case resp3::type::simple_error: ec = adapter::error::simple_error; return;
      case resp3::type::blob_error: ec = adapter::error::blob_error; return;
      case resp3::type::null: ec = adapter::error::null; return;
      default: return;
   }
}

// For optional responses.
void set_on_resp3_error2(resp3::type t, boost::system::error_code& ec)
{
   switch (t) {
      case resp3::type::simple_error: ec = adapter::error::simple_error; return;
      case resp3::type::blob_error: ec = adapter::error::blob_error; return;
      default: return;
   }
}

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
    *  \param t The RESP3 type.
    *
    *  \param n When t is an aggregate type this will contain its size
    *     (see also element_multiplicity) for simple types this is always 1.
    *
    *  \param depth The element depth in the tree.
    *
    *  \param value A pointer to the data.
    *
    *  \param size The size of value.
    */
   void
   operator()(
      resp3::type t,
      std::size_t aggregate_size,
      std::size_t depth,
      char const* value,
      std::size_t size,
      boost::system::error_code&)
      {
	 result_->emplace_back(t, aggregate_size, depth, std::string{value, size});
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
      resp3::type t,
      std::size_t aggregate_size,
      std::size_t depth,
      char const* value,
      std::size_t data_size,
      boost::system::error_code&)
   {
     result_->data_type = t;
     result_->aggregate_size = aggregate_size;
     result_->depth = depth;
     result_->value.assign(value, data_size);
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
	 ec = adapter::error::expects_simple_type;
	 return;
      }

      assert(aggregate_size == 1);
      from_string(*result_, value, data_size, ec);
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
      resp3::type t,
      std::size_t aggregate_size,
      std::size_t depth,
      char const* value,
      std::size_t data_size,
      boost::system::error_code& ec)
   {
      set_on_resp3_error2(t, ec);
      if (ec)
        return;

      if (is_aggregate(t)) {
	 ec = adapter::error::expects_simple_type;
	 return;
      }

      assert(aggregate_size == 1);

      if (depth != 0) {
	 ec = adapter::error::nested_unsupported;
	 return;
      }

      if (t == resp3::type::null)
         return;

      if (!result_->has_value())
        *result_ = T{};

      from_string(result_->value(), value, data_size, ec);
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
   operator()(resp3::type t,
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
	    ec = adapter::error::nested_unsupported;
	    return;
	 }

         auto const m = element_multiplicity(t);
         result_->resize(m * aggregate_size);
         ++i_;
      } else {
         if (aggregate_size != 1) {
            ec = adapter::error::nested_unsupported;
            return;
         }

         from_string(result_->at(i_), value, data_size, ec);
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
   operator()(resp3::type t,
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
	 if (depth != 0 && depth != 1) {
	    ec = adapter::error::nested_unsupported;
	    return;
	 }
         return;
      }

      if (aggregate_size != 1) {
         ec = adapter::error::nested_unsupported;
         return;
      }

      if (depth < 1) {
	 ec = adapter::error::nested_unsupported;
	 return;
      }

      result_->push_back({});
      from_string(result_->back(), value, data_size, ec);
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
   operator()(resp3::type t,
       std::size_t aggregate_size,
       std::size_t depth,
       char const* value,
       std::size_t data_size,
       boost::system::error_code& ec)
   {
      set_on_resp3_error(t, ec);
      if (ec)
         return;

      if (t == resp3::type::set) {
        assert(depth == 0);
        return;
      }

      assert(!is_aggregate(t));

      // TODO: This should cause an error not an assertion.
      assert(depth == 1);
      assert(aggregate_size == 1);

      typename Container::key_type obj;
      from_string(obj, value, data_size, ec);
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
   operator()(resp3::type t,
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
        assert(t == resp3::type::map);
	if (depth != 0 && depth != 1) {
	   ec = adapter::error::nested_unsupported;
	   return;
	}
        return;
      }

      if (aggregate_size != 1) {
         ec = adapter::error::nested_unsupported;
         return;
      }

      if (depth < 1) {
	 ec = adapter::error::nested_unsupported;
	 return;
      }

      if (on_key_) {
         typename Container::key_type obj;
         from_string(obj, value, data_size, ec);
         current_ = result_->insert(current_, {std::move(obj), {}});
      } else {
         typename Container::mapped_type obj;
         from_string(obj, value, data_size, ec);
         current_->second = std::move(obj);
      }

      on_key_ = !on_key_;
   }
};

} // detail
} // adapter
} // aedis
