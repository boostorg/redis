//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Nikolai Vladimirov (nvladimirov.work@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_RESP3_FLAT_TREE_HPP
#define BOOST_REDIS_RESP3_FLAT_TREE_HPP

#include <boost/redis/resp3/node.hpp>
#include <boost/redis/resp3/tree.hpp>

#include <cstddef>
#include <memory>

namespace boost::redis {

namespace adapter::detail {
template <class> class general_aggregate;
}  // namespace adapter::detail

namespace resp3 {

namespace detail {
struct buffer_repr {
   std::unique_ptr<char[]> data;
   std::size_t size = 0u;
   std::size_t capacity = 0u;
};
}  // namespace detail

/** @brief A generic-response that stores data contiguously
 *
 * Similar to the @ref boost::redis::resp3::tree but data is
 * stored contiguously.
 */
struct flat_tree {
public:
   /// Default constructor
   flat_tree() = default;

   /// Move constructor
   flat_tree(flat_tree&&) noexcept = default;

   /// Copy constructor
   flat_tree(flat_tree const& other);

   /// Move assignment
   flat_tree& operator=(flat_tree&& other) = default;

   /// Copy assignment
   flat_tree& operator=(const flat_tree& other);

   friend void swap(flat_tree&, flat_tree&);

   friend bool operator==(flat_tree const&, flat_tree const&);

   friend bool operator!=(flat_tree const&, flat_tree const&);

   /** @brief Reserve capacity
    *
    *  Reserve memory for incoming data.
    *
    *  @param bytes Number of bytes to reserve for data.
    *  @param nodes Number of nodes to reserve.
    */
   void reserve(std::size_t bytes, std::size_t nodes);

   /** @brief Clear both the data and the node buffers
    *  
    *  @Note: A `boost::redis:.flat_tree` can contain the
    *  response to multiple Redis commands and server pushes. Calling
    *  this function will erase everything contained in it.
    */
   void clear();

   /// Returns the size of the data buffer
   auto data_size() const noexcept -> std::size_t { return data_.size; }

   /// Returns the RESP3 response
   auto get_view() const -> view_tree const& { return view_tree_; }

   /** @brief Returns the number of times reallocation took place
    *
    *  This function returns how many reallocations were performed and
    *  can be useful to determine how much memory to reserve upfront.
    */
   auto get_reallocs() const noexcept -> std::size_t { return reallocs_; }

   /// Returns the number of complete RESP3 messages contained in this object.
   std::size_t get_total_msgs() const noexcept { return total_msgs_; }

private:
   template <class> friend class adapter::detail::general_aggregate;

   void notify_done() { ++total_msgs_; }

   // Push a new node to the response
   void push(node_view const& node);

   void grow(std::size_t target_size);

   detail::buffer_repr data_;
   view_tree view_tree_;
   std::size_t reallocs_ = 0u;
   std::size_t total_msgs_ = 0u;
};

/// Equality operator
bool operator==(flat_tree const&, flat_tree const&);

/// Inequality operator
bool operator!=(flat_tree const&, flat_tree const&);

}  // namespace resp3
}  // namespace boost::redis

#endif  // BOOST_REDIS_RESP3_FLAT_TREE_HPP
