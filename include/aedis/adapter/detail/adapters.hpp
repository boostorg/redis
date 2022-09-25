/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_ADAPTER_ADAPTERS_HPP
#define AEDIS_ADAPTER_ADAPTERS_HPP

#include <set>
#include <optional>
#include <unordered_set>
#include <forward_list>
#include <system_error>
#include <map>
#include <unordered_map>
#include <list>
#include <deque>
#include <vector>
#include <array>

#include <boost/assert.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/utility/string_view.hpp>

#include <aedis/error.hpp>
#include <aedis/resp3/type.hpp>
#include <aedis/resp3/request.hpp>
#include <aedis/resp3/detail/parser.hpp>
#include <aedis/resp3/node.hpp>

namespace aedis::adapter::detail {

inline
auto parse_double(
   char const* data,
   std::size_t size,
   boost::system::error_code& ec) -> double
{
   static constexpr boost::spirit::x3::real_parser<double> p{};
   double ret = 0;
   if (!parse(data, data + size, p, ret))
      ec = error::not_a_double;

   return ret;
}

// Serialization.

template <class T>
typename std::enable_if<std::is_integral<T>::value, void>::type
from_bulk(
   T& i,
   boost::string_view sv,
   boost::system::error_code& ec)
{
   i = resp3::detail::parse_uint(sv.data(), sv.size(), ec);
}

inline
void from_bulk(
   bool& t,
   boost::string_view sv,
   boost::system::error_code&)
{
   t = *sv.data() == 't';
}

inline
void from_bulk(
   double& d,
   boost::string_view sv,
   boost::system::error_code& ec)
{
   d = parse_double(sv.data(), sv.size(), ec);
}

template <class CharT, class Traits, class Allocator>
void
from_bulk(
   std::basic_string<CharT, Traits, Allocator>& s,
   boost::string_view sv,
   boost::system::error_code&)
{
  s.append(sv.data(), sv.size());
}

//================================================

inline
void set_on_resp3_error(resp3::type t, boost::system::error_code& ec)
{
   switch (t) {
      case resp3::type::simple_error: ec = error::resp3_simple_error; return;
      case resp3::type::blob_error: ec = error::resp3_blob_error; return;
      case resp3::type::null: ec = error::resp3_null; return;
      default: return;
   }
}

template <class Result>
class general_aggregate {
private:
   Result* result_;

public:
   explicit general_aggregate(Result* c = nullptr): result_(c) {}
   void operator()(resp3::node<boost::string_view> const& n, boost::system::error_code& ec)
   {
      result_->push_back({n.data_type, n.aggregate_size, n.depth, std::string{std::cbegin(n.value), std::cend(n.value)}});
      set_on_resp3_error(n.data_type, ec);
   }
};

template <class Node>
class general_simple {
private:
   Node* result_;

public:
   explicit general_simple(Node* t = nullptr) : result_(t) {}

   void operator()(resp3::node<boost::string_view> const& n, boost::system::error_code& ec)
   {
      result_->data_type = n.data_type;
      result_->aggregate_size = n.aggregate_size;
      result_->depth = n.depth;
      result_->value.assign(n.value.data(), n.value.size());
      set_on_resp3_error(n.data_type, ec);
   }
};

template <class Result>
class simple_impl {
public:
   void on_value_available(Result&) {}

   void
   operator()(
      Result& result,
      resp3::node<boost::string_view> const& n,
      boost::system::error_code& ec)
   {
      set_on_resp3_error(n.data_type, ec);
      if (ec)
         return;

      if (is_aggregate(n.data_type)) {
         ec = error::expects_resp3_simple_type;
         return;
      }

      from_bulk(result, n.value, ec);
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
      resp3::node<boost::string_view> const& nd,
      boost::system::error_code& ec)
   {
      set_on_resp3_error(nd.data_type, ec);
      if (ec)
         return;

      if (is_aggregate(nd.data_type)) {
         if (nd.data_type != resp3::type::set)
            ec = error::expects_resp3_set;
         return;
      }

      BOOST_ASSERT(nd.aggregate_size == 1);

      if (nd.depth < 1) {
	 ec = error::expects_resp3_set;
	 return;
      }

      typename Result::key_type obj;
      from_bulk(obj, nd.value, ec);
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
      resp3::node<boost::string_view> const& nd,
      boost::system::error_code& ec)
   {
      set_on_resp3_error(nd.data_type, ec);
      if (ec)
         return;

      if (is_aggregate(nd.data_type)) {
         if (element_multiplicity(nd.data_type) != 2)
           ec = error::expects_resp3_map;
         return;
      }

      BOOST_ASSERT(nd.aggregate_size == 1);

      if (nd.depth < 1) {
	 ec = error::expects_resp3_map;
	 return;
      }

      if (on_key_) {
         typename Result::key_type obj;
         from_bulk(obj, nd.value, ec);
         current_ = result.insert(current_, {std::move(obj), {}});
      } else {
         typename Result::mapped_type obj;
         from_bulk(obj, nd.value, ec);
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
      resp3::node<boost::string_view> const& nd,
      boost::system::error_code& ec)
   {
      set_on_resp3_error(nd.data_type, ec);
      if (ec)
         return;

      if (is_aggregate(nd.data_type)) {
         auto const m = element_multiplicity(nd.data_type);
         result.reserve(result.size() + m * nd.aggregate_size);
      } else {
         result.push_back({});
         from_bulk(result.back(), nd.value, ec);
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
      resp3::node<boost::string_view> const& nd,
      boost::system::error_code& ec)
   {
      set_on_resp3_error(nd.data_type, ec);
      if (ec)
         return;

      if (is_aggregate(nd.data_type)) {
	 if (i_ != -1) {
            ec = error::nested_aggregate_not_supported;
            return;
         }

         if (result.size() != nd.aggregate_size * element_multiplicity(nd.data_type)) {
            ec = error::incompatible_size;
            return;
         }
      } else {
         if (i_ == -1) {
            ec = error::expects_resp3_aggregate;
            return;
         }

         BOOST_ASSERT(nd.aggregate_size == 1);
         from_bulk(result.at(i_), nd.value, ec);
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
      resp3::node<boost::string_view> const& nd,
      boost::system::error_code& ec)
   {
      set_on_resp3_error(nd.data_type, ec);
      if (ec)
         return;

      if (!is_aggregate(nd.data_type)) {
        BOOST_ASSERT(nd.aggregate_size == 1);
        if (nd.depth < 1) {
           ec = error::expects_resp3_aggregate;
           return;
        }

        result.push_back({});
        from_bulk(result.back(), nd.value, ec);
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
   explicit wrapper(Result* t = nullptr) : result_(t)
      { impl_.on_value_available(*result_); }

   void
   operator()(
      resp3::node<boost::string_view> const& nd,
      boost::system::error_code& ec)
   {
      BOOST_ASSERT(result_);
      impl_(*result_, nd, ec);
   }
};

template <class T>
class wrapper<std::optional<T>> {
private:
   std::optional<T>* result_;
   typename impl_map<T>::type impl_{};

public:
   explicit wrapper(std::optional<T>* o = nullptr) : result_(o) {}

   void
   operator()(
      resp3::node<boost::string_view> const& nd,
      boost::system::error_code& ec)
   {
      if (nd.data_type == resp3::type::null)
         return;

      if (!result_->has_value()) {
        *result_ = T{};
        impl_.on_value_available(result_->value());
      }

      impl_(result_->value(), nd, ec);
   }
};

} // aedis::adapter:.detail

#endif // AEDIS_ADAPTER_ADAPTERS_HPP
