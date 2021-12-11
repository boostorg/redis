/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/command.hpp>
#include <aedis/resp3/type.hpp>
#include <aedis/resp3/request.hpp>

#include <set>
#include <list>
#include <deque>
#include <vector>
#include <charconv>

namespace aedis {
namespace resp3 {

/** \brief A node in the response tree.
 */
struct node {
   enum class dump_format {raw, clean};

   /// The number of children node is parent of.
   std::size_t size;

   /// The depth of this node in the response tree.
   std::size_t depth;

   /// The RESP3 type  of the data in this node.
   type data_type;

   /// The data. For aggregate data types this is always empty.
   std::string data;

   /// Converts the node to a string and appends to out.
   void dump(dump_format format, int indent, std::string& out) const;
};

using storage_type = std::vector<node>;

/** \brief A general pupose redis response class
  
    A pre-order-view of the response tree.

 */
struct response_adapter {
   storage_type* data_;

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
      { data_->emplace_back(n, depth, t, std::string{data, size}); }
};

std::string
dump(
   storage_type const& obj,
   node::dump_format format = node::dump_format::clean,
   int indent = 3);

/// Equality comparison for a node.
bool operator==(node const& a, node const& b);

/** Writes the text representation of node to the output stream.
 *  
 *  NOTE: Binary data is not converted to text.
 */
std::ostream& operator<<(std::ostream& os, node const& o);

/** Writes the text representation of the response to the output
 *  stream the response to the output stream.
 */
std::ostream& operator<<(std::ostream& os, storage_type const& r);

std::ostream& operator<<(std::ostream& os, storage_type const& r);

struct adapter_ignore {
   void operator()(type, std::size_t, std::size_t, char const*, std::size_t) { }
};

namespace detail
{

inline
void from_string(int& i, char const* data, std::size_t data_size)
{
  auto r = std::from_chars(data, data + data_size, i);
  if (r.ec == std::errc::invalid_argument)
     throw std::runtime_error("from_chars: Unable to convert");
}

inline
void from_string(std::string& s, char const* data, std::size_t data_size)
{
  s = std::string{data, data_size};
}

/** A response that parses the result of a response directly in an int
    variable thus avoiding unnecessary copies. The same reasoning can
    be applied for keys containing e.g. json strings.
 */
template <class T>
struct adapter_int {
   T* result;

   void
   operator()(type t,
       std::size_t aggregate_size,
       std::size_t depth,
       char const* data,
       std::size_t data_size)
   {
      auto r = std::from_chars(data, data + data_size, *result);
      if (r.ec == std::errc::invalid_argument)
         throw std::runtime_error("from_chars: Unable to convert");
   }
};

template <class T, class Traits, class Allocator>
struct adapter_string {
   std::basic_string<T, Traits, Allocator>* result;

   void
   operator()(type t,
      std::size_t aggregate_size,
      std::size_t depth,
      char const* data,
      std::size_t data_size)
   {
      assert(aggregate_size == 1);
      assert(depth == 0);
      result->assign(data, data_size);
   }
};
/** A response type that parses the response directly in a
 */
template <class T, class Allocator>
class adapter_vector {
private:
   int i_ = 0;

public:
   std::vector<T, Allocator>* result_;

   adapter_vector(std::vector<T, Allocator>& v)
      : result_{&v} {}

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

template <class ListContainer>
struct adapter_list {
   ListContainer* result_;

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

template <class T, class Compare, class Allocator>
class adapter_set {
public:
   using container_type = std::set<T, Compare, Allocator>;

private:
   container_type* result_;
   container_type::iterator hint_;

public:
   adapter_set(container_type& c)
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

      T obj;
      from_string(obj, data, data_size);
      if (hint_ == std::end(*result_)) {
         auto const ret = result_->insert(std::move(obj));
         hint_ = ret.first;
      } else {
         hint_ = result_->insert(hint_, std::move(obj));
      }
   }
};

} // detail

template <class>
struct adapter_factory;

template <>
struct adapter_factory<int>
{
   using type = detail::adapter_int<int>;

   static auto make(int& i) noexcept
     { return type{&i}; }
};

template <class T, class Traits, class Allocator>
struct adapter_factory<std::basic_string<T, Traits, Allocator>>
{
   using type = detail::adapter_string<T, Traits, Allocator>;

   static auto make(std::basic_string<T, Traits, Allocator>& s) noexcept
     { return type{&s}; }
};

template <class T, class Allocator>
struct adapter_factory<std::vector<T, Allocator>>
{
   using type = detail::adapter_vector<T, Allocator>;

   static auto make(std::vector<T, Allocator>& v) noexcept
     { return type{v}; }
};

template <class T, class Allocator>
struct adapter_factory<std::list<T, Allocator>>
{
   using type = detail::adapter_list<std::list<T, Allocator>>;

   static auto make(std::list<T, Allocator>& v) noexcept
     { return type{&v}; }
};

template <class T, class Allocator>
struct adapter_factory<std::deque<T, Allocator>>
{
   using type = detail::adapter_list<std::deque<T, Allocator>>;

   static auto make(std::deque<T, Allocator>& v) noexcept
     { return type{&v}; }
};

template <class T, class Compare, class Allocator>
struct adapter_factory<std::set<T, Compare, Allocator>>
{
   using type = detail::adapter_set<T, Compare, Allocator>;

   static auto make(std::set<T, Compare, Allocator>& s) noexcept
     { return type{s}; }
};

template <>
struct adapter_factory<void>
{
   using type = adapter_ignore;

   static auto make() noexcept
     { return type{}; }
};

inline
adapter_factory<void>::type
adapt() noexcept
{
  return adapter_factory<void>::make();
}

template<class T>
adapter_factory<T>::type
adapt(T& t) noexcept
{
  return adapter_factory<T>::make(t);
}

} // resp3
} // aedis
