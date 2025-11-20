//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Nikolai Vladimirov (nvladimirov.work@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/resp3/flat_tree.hpp>
#include <boost/redis/resp3/node.hpp>
#include <boost/redis/resp3/tree.hpp>

#include <boost/assert.hpp>

#include <algorithm>
#include <cstring>
#include <string_view>

namespace boost::redis::resp3 {

namespace detail {

inline std::size_t compute_capacity(std::size_t requested_cap, std::size_t current_cap)
{
   // Prevent many small allocations when starting from an empty buffer
   requested_cap = (std::max)(requested_cap, static_cast<std::size_t>(512u));
   return (std::max)(requested_cap, 2 * current_cap);
}

inline std::string_view rebase_string(
   std::string_view value,
   const char* old_base,
   const char* new_base)
{
   if (value.empty())
      return value;
   const auto offset = value.data() - old_base;
   BOOST_ASSERT(offset >= 0);
   return {new_base + offset, value.size()};
}

inline void rebase_strings(view_tree& nodes, const char* old_base, const char* new_base)
{
   for (auto& nd : nodes)
      nd.value = rebase_string(nd.value, old_base, new_base);
}

}  // namespace detail

flat_tree::buffer flat_tree::copy(const buffer& other)
{
   buffer res{{}, other.size, other.capacity};

   if (other.data) {
      BOOST_ASSERT(other.capacity > 0u);
      res.data.reset(new char[other.capacity]);
      std::memcpy(res.data.get(), other.data.get(), other.size);
   }

   return res;
}

void flat_tree::grow(std::size_t new_capacity)
{
   BOOST_ASSERT(new_capacity > data_.capacity);

   // Compute the actual capacity that we will be using
   new_capacity = detail::compute_capacity(new_capacity, data_.capacity);

   // Allocate space
   std::unique_ptr<char[]> new_buffer{new char[new_capacity]};

   if (data_.data) {
      BOOST_ASSERT(data_.capacity > 0u);

      // Rebase strings. This operation must be performed after allocating
      // the new buffer and before freeing the old one. Otherwise, we're
      // comparing invalid pointers, which is UB.
      const char* data_before = data_.data.get();
      char* data_after = new_buffer.get();
      detail::rebase_strings(view_tree_, data_before, data_after);

      // Copy contents
      std::memcpy(data_after, data_before, data_.size);
   }

   // Replace the buffer
   data_.data = std::move(new_buffer);
   data_.capacity = new_capacity;
   ++reallocs_;
}

flat_tree::flat_tree(flat_tree const& other)
: data_{copy(other.data_)}
, view_tree_{other.view_tree_}
, reallocs_{0u}
, total_msgs_{other.total_msgs_}
{
   detail::rebase_strings(view_tree_, other.data_.data.get(), data_.data.get());
}

flat_tree& flat_tree::operator=(const flat_tree& other)
{
   if (this != &other) {
      // Copy the data
      if (data_.capacity >= other.data_.capacity) {
         std::memcpy(data_.data.get(), other.data_.data.get(), other.data_.size);
         data_.size = other.data_.size;
      } else {
         data_ = copy(other.data_);
      }

      // Copy the nodes
      view_tree_ = other.view_tree_;
      detail::rebase_strings(view_tree_, other.data_.data.get(), data_.data.get());

      // Copy the other fields
      reallocs_ = other.reallocs_;
      total_msgs_ = other.total_msgs_;
   }

   return *this;
}

void flat_tree::reserve(std::size_t bytes, std::size_t nodes)
{
   // Space for the strings
   if (bytes > data_.capacity) {
      grow(bytes);
   }

   // Space for the nodes
   view_tree_.reserve(nodes);
}

void flat_tree::clear()
{
   data_.size = 0u;
   view_tree_.clear();
   reallocs_ = 0u;
   total_msgs_ = 0u;
}

void flat_tree::push(node_view const& nd)
{
   // Add the string first
   const std::size_t offset = data_.size;
   const std::size_t new_size = data_.size + nd.value.size();
   if (new_size > data_.capacity) {
      grow(new_size);
   }
   std::memcpy(data_.data.get() + offset, nd.value.data(), nd.value.size());
   data_.size = new_size;

   // Add the node
   view_tree_.push_back({
      nd.data_type,
      nd.aggregate_size,
      nd.depth,
      std::string_view{data_.data.get() + offset, nd.value.size()}
   });
}

bool operator==(flat_tree const& a, flat_tree const& b)
{
   // reallocs is not taken into account.
   // data is already taken into account by comparing the nodes.
   return a.view_tree_ == b.view_tree_ && a.total_msgs_ == b.total_msgs_;
}

bool operator!=(flat_tree const& a, flat_tree const& b) { return !(a == b); }

}  // namespace boost::redis::resp3
