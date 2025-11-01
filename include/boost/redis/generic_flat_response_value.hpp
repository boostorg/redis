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
#include <vector>

namespace boost::redis {

namespace adapter::detail {
   template <class> class general_aggregate;
}

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

   /// Returns the RESP3 response
   auto get_view() const -> resp3::view_response const&
      { return view_resp_; }

   /** @brief Returns the number of times reallocation took place
    *
    *  This function returns how many reallocations were performed and
    *  can be useful to determine how much memory to reserve upfront.
    */
   auto get_reallocs() const noexcept -> std::size_t
      { return reallocs_; }

   /// Returns the number of complete RESP3 messages contained in this object.
   std::size_t get_total_msgs() const noexcept
      { return total_msgs_; }

private:
   template <class> friend class adapter::detail::general_aggregate;

   // Notify the object that all nodes were pushed.
   void notify_done();

   // Push a new node to the response
   void push(resp3::node_view const& node);

   void add_node_impl(resp3::node_view const& node);

   // Range into the data buffer.
   struct range {
      std::size_t offset;
      std::size_t size;
   };

   std::string data_;
   resp3::view_response view_resp_;
   std::vector<range> ranges_;
   std::size_t pos_ = 0u;
   std::size_t reallocs_ = 0u;
   std::size_t total_msgs_ = 0u;
};

}  // namespace boost::redis

#endif  // BOOST_REDIS_FLAT_RESPONSE_HPP
