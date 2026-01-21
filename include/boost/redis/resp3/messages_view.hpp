//
// Copyright (c) 2026 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_RESP3_MESSAGES_VIEW_HPP
#define BOOST_REDIS_RESP3_MESSAGES_VIEW_HPP

#include <boost/redis/resp3/node.hpp>

#include <boost/core/span.hpp>

#include <cstddef>
#include <iterator>

namespace boost::redis::resp3 {

class messages_view {
   span<const node_view> nodes_;

   static std::size_t compute_message_size(span<const node_view> nodes);

public:
   // TODO: explicit?
   messages_view(span<const node_view> nodes) noexcept
   : nodes_{nodes}
   { }

   class iterator {
      const node_view* data_;
      const node_view* end_;  // required to iterate
      std::size_t size_;      // of the found range

      friend class messages_view;

      iterator(const node_view* data, const node_view* end, std::size_t size) noexcept
      : data_{data}
      , end_{end}
      , size_{size}
      { }

      void increment()
      {
         data_ += size_;
         size_ = compute_message_size({data_, end_});
      }

   public:
      using value_type = span<const node_view>;
      using reference = span<const node_view>;
      using pointer = span<const node_view>;
      using difference_type = std::ptrdiff_t;
      using iterator_category = std::forward_iterator_tag;

      iterator() = default;

      reference operator*() const noexcept { return {data_, size_}; }

      pointer operator->() const noexcept { return {data_, size_}; }

      iterator& operator++() noexcept
      {
         increment();
         return *this;
      }

      iterator operator++(int) noexcept
      {
         iterator res{*this};
         increment();
         return res;
      }

      bool operator==(const iterator& rhs) const noexcept { return data_ == rhs.data_; }
      bool operator!=(const iterator& rhs) const noexcept { return !(*this == rhs); }
   };

   iterator begin() const noexcept
   {
      return {nodes_.begin(), nodes_.end(), compute_message_size(nodes_)};
   }
   iterator end() const noexcept { return {nodes_.end(), nodes_.end(), 0u}; }
   bool empty() const noexcept { return nodes_.empty(); }
};

}  // namespace boost::redis::resp3

#endif  // BOOST_REDIS_RESP3_FLAT_TREE_HPP
