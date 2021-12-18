/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/command.hpp>
#include <aedis/resp3/type.hpp>
#include <aedis/resp3/node.hpp>
#include <aedis/resp3/serializer.hpp>

#include <set>
#include <map>
#include <list>
#include <deque>
#include <vector>
#include <charconv>

namespace aedis {
namespace resp3 {

namespace detail
{

struct adapter_ignore {
   void operator()(type, std::size_t, std::size_t, char const*, std::size_t) { }
};

template <class T>
typename std::enable_if<std::is_integral<T>::value, void>::type
from_string(T& i, char const* data, std::size_t data_size)
{
  auto const r = std::from_chars(data, data + data_size, i);
  assert(r.ec != std::errc::invalid_argument);
}

template <class CharT, class Traits, class Allocator>
void
from_string(
   std::basic_string<CharT, Traits, Allocator>& s,
   char const* data,
   std::size_t data_size)
{
  s.assign(data, data_size);
}

/** \brief A general pupose redis response class
  
    A pre-order-view of the response tree.
 */
template <class Container>
class adapter_general {
private:
   Container* result_;

public:
   adapter_general(Container& c = nullptr): result_(&c) {}

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
   void operator()(type t, std::size_t n, std::size_t depth, char const* data, std::size_t size)
      { result_->emplace_back(n, depth, t, std::string{data, size}); }
};

// Adapter for simple data types.
template <class T>
class adapter_simple {
private:
   T* result_;

public:
   adapter_simple(T& t) : result_(&t) {}

   void
   operator()(
      type t,
      std::size_t aggregate_size,
      std::size_t depth,
      char const* data,
      std::size_t data_size)
   {
     assert(!is_aggregate(t));
     from_string(*result_, data, data_size);
   }
};

/** A response type that parses the response directly in a
 */
template <class Container>
class adapter_vector {
private:
   int i_ = 0;
   Container* result_;

public:
   adapter_vector(Container& v) : result_{&v} {}

   void
   operator()(type t,
       std::size_t aggregate_size,
       std::size_t depth,
       char const* data,
       std::size_t data_size)
   {
      if (is_aggregate(t)) {
         auto const m = element_multiplicity(t);
         result_->resize(m * aggregate_size);
      } else {
         from_string(result_->at(i_), data, data_size);
         ++i_;
      }
   }
};

template <class Container>
class adapter_list {
private:
   Container* result_;

public:
   adapter_list(Container& ref): result_(&ref) {}

   void
   operator()(type t,
       std::size_t aggregate_size,
       std::size_t depth,
       char const* data,
       std::size_t data_size)
   {
      if (is_aggregate(t)) {
        assert(depth == 0);
        return;
      }

      assert(depth == 1);
      assert(aggregate_size == 1);
      result_->push_back({});
      from_string(result_->back(), data, data_size);
   }
};

template <class Container>
class adapter_set {
private:
   Container* result_;
   Container::iterator hint_;

public:
   adapter_set(Container& c)
   : result_(&c)
   , hint_(std::end(c))
   {}

   void
   operator()(type t,
       std::size_t aggregate_size,
       std::size_t depth,
       char const* data,
       std::size_t data_size)
   {
      if (t == type::set) {
        assert(depth == 0);
        return;
      }

      assert(!is_aggregate(t));

      assert(depth == 1);
      assert(aggregate_size == 1);

      typename Container::key_type obj;
      from_string(obj, data, data_size);
      if (hint_ == std::end(*result_)) {
         auto const ret = result_->insert(std::move(obj));
         hint_ = ret.first;
      } else {
         hint_ = result_->insert(hint_, std::move(obj));
      }
   }
};

template <class Container>
class adapter_map {
private:
   Container* result_;
   Container::iterator current_;
   bool on_key_ = true;

public:
   adapter_map(Container& c)
   : result_(&c)
   , current_(std::end(c))
   {}

   void
   operator()(type t,
       std::size_t aggregate_size,
       std::size_t depth,
       char const* data,
       std::size_t data_size)
   {
      if (t == type::map) {
        assert(depth == 0);
        return;
      }

      assert(!is_aggregate(t));
      assert(depth == 1);
      assert(aggregate_size == 1);

      if (on_key_) {
         typename Container::key_type obj;
         from_string(obj, data, data_size);
         current_ = result_->insert(current_, {std::move(obj), {}});
      } else {
         typename Container::mapped_type obj;
         from_string(obj, data, data_size);
         current_->second = std::move(obj);
      }

      on_key_ = !on_key_;
   }
};

} // detail
} // resp3
} // aedis
