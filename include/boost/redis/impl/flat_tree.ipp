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
#include <cstddef>
#include <cstring>
#include <string_view>

namespace boost::redis::resp3 {

namespace detail {

// --- Operations in flat_buffer ---

// Copies the entire buffer
inline flat_buffer copy(const flat_buffer& other)
{
   flat_buffer res{{}, other.size, other.capacity, other.reallocs};

   if (other.data) {
      BOOST_ASSERT(other.capacity > 0u);
      res.data.reset(new char[other.capacity]);
      std::memcpy(res.data.get(), other.data.get(), other.size);
   }

   return res;
}

// Grows the buffer until reaching a target size
template <class Callable>
inline void grow(flat_buffer& buff, std::size_t new_capacity, Callable on_realloc)
{
   if (new_capacity <= buff.capacity)
      return;

   // Compute the actual capacity that we will be using
   // Prevent many small allocations when starting from an empty buffer
   new_capacity = (std::max)(new_capacity, static_cast<std::size_t>(512u));
   new_capacity = (std::max)(new_capacity, 2 * buff.capacity);

   // Allocate space
   std::unique_ptr<char[]> new_buffer{new char[new_capacity]};

   if (buff.size > 0u) {
      // Copy any data into the newly allocated space
      const char* data_before = buff.data.get();
      char* data_after = new_buffer.get();
      std::memcpy(data_after, data_before, buff.size);

      // Inform the caller that there has been a reallocation
      on_realloc(data_before, static_cast<const char*>(data_after));
   }

   // Replace the buffer. Note that size hasn't changed here
   buff.data = std::move(new_buffer);
   buff.capacity = new_capacity;
   ++buff.reallocs;
}

// Appends a string to the buffer. There should be enough size for it.
inline std::string_view append(flat_buffer& buff, std::string_view value)
{
   const std::size_t offset = buff.size;
   const std::size_t new_size = buff.size + value.size();
   BOOST_ASSERT(buff.capacity >= new_size);
   if (!value.empty()) {
      std::memmove(buff.data.get() + offset, value.data(), value.size());
   }
   buff.size = new_size;
   return {buff.data.get() + offset, value.size()};
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

void flat_tree::reserve_data(std::size_t new_capacity)
{
   detail::grow(data_, new_capacity, [this](const char* old_base, const char* new_base) {
      detail::rebase_strings(view_tree_, old_base, new_base);
   });
}

flat_tree::flat_tree(flat_tree const& other)
: data_{detail::copy(other.data_)}
, view_tree_{other.view_tree_}
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
      total_msgs_ = other.total_msgs_;
   }

   return *this;
}

void flat_tree::reserve(std::size_t bytes, std::size_t nodes)
{
   // Space for the strings
   reserve_data(bytes);

   // Space for the nodes
   view_tree_.reserve(nodes);
}

void flat_tree::clear()
{
   data_.size = 0u;
   view_tree_.clear();
   total_msgs_ = 0u;
}

void flat_tree::push(node_view const& nd)
{
   // Add the string
   reserve_data(data_.size + nd.value.size());
   const std::string_view str = detail::append(data_, nd.value);

   // Add the node
   view_tree_.push_back({
      nd.data_type,
      nd.aggregate_size,
      nd.depth,
      str,
   });
}

bool operator==(flat_tree const& a, flat_tree const& b)
{
   // data is already taken into account by comparing the nodes.
   return a.view_tree_ == b.view_tree_ && a.total_msgs_ == b.total_msgs_;
}

bool operator!=(flat_tree const& a, flat_tree const& b) { return !(a == b); }

}  // namespace boost::redis::resp3
