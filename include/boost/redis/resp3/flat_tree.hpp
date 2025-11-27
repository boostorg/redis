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

struct flat_buffer {
   std::unique_ptr<char[]> data;
   std::size_t size = 0u;
   std::size_t capacity = 0u;
   std::size_t reallocs = 0u;
};

}  // namespace detail

/** @brief A generic response that stores data contiguously.
 *
 * Implements a container of RESP3 nodes. It's similar to @ref boost::redis::resp3::tree,
 * but node data is stored contiguously. This allows for amortized no allocations
 * when re-using `flat_tree` objects. Like `tree`, it can contain the response
 * to several Redis commands or several server pushes. Use @ref get_total_msgs
 * to obtain how many responses this object contains.
 *
 * Objects are typically created by the user and passed to @ref connection::async_exec
 * to be used as response containers. Call @ref get_view to access the actual RESP3 nodes.
 * Once populated, `flat_tree` can't be modified, except for @ref clear and assignment.
 *
 * A `flat_tree` is conceptually similar to a pair of `std::vector` objects, one holding
 * @ref resp3::node_view objects, and another owning the the string data that these views
 * point to. The node capacity and the data capacity are the capacities of these two vectors.
 */
class flat_tree {
public:
   /**
    * @brief Default constructor.
    *
    * Constructs an empty tree, with no nodes, zero node capacity and zero data capacity.
    *
    * @par Exception safety
    * No-throw guarantee.
    */
   flat_tree() = default;

   /**
    * @brief Move constructor.
    *
    * Constructs a tree by taking ownership of the nodes in `other`.
    *
    * @par Object lifetimes
    * References to the nodes and strings in `other` remain valid.
    *
    * @par Exception safety
    * No-throw guarantee.
    */
   flat_tree(flat_tree&& other) noexcept = default;

   /**
    * @brief Copy constructor.
    *
    * Constructs a tree by copying the nodes in `other`. After the copy,
    * `*this` and `other` have independent lifetimes (usual copy semantics).
    *
    * @par Exception safety
    * Strong guarantee. Memory allocations might throw.
    */
   flat_tree(flat_tree const& other);

   /**
    * @brief Move assignment.
    *
    * Replaces the nodes in `*this` by taking ownership of the nodes in `other`.
    * `other` is left in a valid but unspecified state.
    *
    * @par Object lifetimes
    * References to the nodes and strings in `other` remain valid.
    * References to the nodes and strings in `*this` are invalidated.
    *
    * @par Exception safety
    * No-throw guarantee.
    */
   flat_tree& operator=(flat_tree&& other) = default;

   /**
    * @brief Copy assignment.
    *
    * Replaces the nodes in `*this` by copying the nodes in `other`.
    * After the copy, `*this` and `other` have independent lifetimes (usual copy semantics).
    *
    * @par Object lifetimes
    * References to the nodes and strings in `*this` are invalidated.
    *
    * @par Exception safety
    * Basic guarantee. Memory allocations might throw.
    */
   flat_tree& operator=(const flat_tree& other);

   friend bool operator==(flat_tree const&, flat_tree const&);

   friend bool operator!=(flat_tree const&, flat_tree const&);

   /** @brief Reserves capacity for incoming data.
    *
    * Adding nodes (e.g. by passing the tree to `async_exec`)
    * won't cause reallocations until the data or node capacities
    * are exceeded, following the usual vector semantics.
    * The implementation might reserve more capacity than the one requested.
    * 
    * @par Object lifetimes
    * References to the nodes and strings in `*this` are invalidated.
    *
    * @par Exception safety
    * Basic guarantee. Memory allocations might throw.
    *
    * @param bytes Number of bytes to reserve for data.
    * @param nodes Number of nodes to reserve.
    */
   void reserve(std::size_t bytes, std::size_t nodes);

   /** @brief Clears the tree so it contains no nodes.
    * 
    * Calling this function removes every node, making
    * @ref get_views return empty and @ref get_total_msgs
    * return zero. It does not modify the object's capacity.
    * 
    * To re-use a `flat_tree` for several requests,
    * use `clear()` before each `async_exec` call.
    *
    * @par Object lifetimes
    * References to the nodes and strings in `*this` are invalidated.
    *
    * @par Exception safety
    * No-throw guarantee.
    */
   void clear() noexcept;

   // TODO: this
   /// Returns the size of the data buffer
   auto data_size() const noexcept -> std::size_t { return data_.size; }

   // TODO: document
   auto data_capacity() const noexcept -> std::size_t { return data_.capacity; }

   /** @brief Returns a vector with the nodes in the tree.
    *
    * This is the main way to access the contents of the tree.
    *
    * @par Exception safety
    * No-throw guarantee.
    */
   auto get_view() const noexcept -> view_tree const& { return view_tree_; }

   /** @brief Returns the number of memory reallocations that took place within this object.
    *
    * This function returns how many reallocations were performed and
    * can be useful to determine how much memory to reserve upfront.
    * 
    * @par Exception safety
    * No-throw guarantee.
    */
   auto get_reallocs() const noexcept -> std::size_t { return data_.reallocs; }

   /** @brief Returns the number of complete RESP3 messages contained in this object.
    *
    * This value is equal to the number of nodes in the tree with a depth of zero.
    *
    * @par Exception safety
    * No-throw guarantee.
    */
   std::size_t get_total_msgs() const noexcept { return total_msgs_; }

private:
   template <class> friend class adapter::detail::general_aggregate;

   void notify_done() { ++total_msgs_; }

   // Push a new node to the response
   void push(node_view const& node);

   detail::flat_buffer data_;
   view_tree view_tree_;
   std::size_t total_msgs_ = 0u;
};

/**
 * @brief Equality operator.
 * @relates flat_tree
 *
 * Two trees are equal if they contain the same nodes in the same order.
 * Capacities are not taken into account.
 *
 * @par Exception safety
 * No-throw guarantee.
 */
bool operator==(flat_tree const&, flat_tree const&);

/**
 * @brief Inequality operator.
 * @relates flat_tree
 *
 * @par Exception safety
 * No-throw guarantee.
 */
inline bool operator!=(flat_tree const& lhs, flat_tree const& rhs) { return !(lhs == rhs); }

}  // namespace resp3
}  // namespace boost::redis

#endif  // BOOST_REDIS_RESP3_FLAT_TREE_HPP
