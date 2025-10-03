//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Nikolai Vladimirov (nvladimirov.work@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_FLAT_RESPONSE_HPP
#define BOOST_REDIS_FLAT_RESPONSE_HPP

#include <boost/redis/adapter/result.hpp>
#include <boost/redis/resp3/node.hpp>

#include <boost/system/error_code.hpp>

#include <string>
#include <tuple>
#include <vector>

namespace boost::redis {

/** @brief A generic-response that stores data contiguously
 *
 * Similar to the @ref boost::redis::generic_response but data is
 * stored contiguously.
 */
struct generic_flat_response_value {
public:
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
    *  @Note: A `boost::redis:.generic_flat_response` can contain the
    *  response to multiple Redis commands and server pushes. Calling
    *  this function will erase everything contained in it.
    */
   void clear();

   /// Returns the size of the data buffer
   auto data_size() const noexcept -> std::size_t
      { return data_.size(); }

   /** @brief Return the RESP3 response
    *  
    *  The data member in each `boost::redis::offset_string` are all
    *  set and therefore safe to use.
    */
   auto get_view() const -> resp3::offset_response const&
      { return view_; }

   // Push a new node to the response
   void push(resp3::node_view const& nd);

   /** @brief Returns the number of times reallocation took place
    *
    *  Each call to the `push` member function might result in a
    *  memory reallocation.  This function returns how many
    *  reallocations were detected and can be useful to determine how
    *  much memory to reserve upfront.
    */
   auto get_reallocs() const noexcept
      { return reallocs_; }

   // Notify the object that all nodes were pushed.
   void notify_done();

   /// Returns the number of complete RESP3 messages contained in this object.
   std::size_t get_total_msgs() const noexcept
      { return total_msgs_; }

private:
   void add_node_impl(resp3::node_view const& nd);

   std::string data_;
   resp3::offset_response view_;
   std::size_t pos_ = 0u;
   std::size_t reallocs_ = 0u;
   std::size_t total_msgs_ = 0u;
};

}  // namespace boost::redis

#endif  // BOOST_REDIS_FLAT_RESPONSE_HPP
