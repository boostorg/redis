/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_RESPONSE_HPP
#define BOOST_REDIS_RESPONSE_HPP

#include <boost/redis/adapter/result.hpp>
#include <boost/redis/resp3/node.hpp>

#include <boost/system/error_code.hpp>

#include <string>
#include <tuple>
#include <vector>

namespace boost::redis {

/// Response with compile-time size.
template <class... Ts>
using response = std::tuple<adapter::result<Ts>...>;

/** @brief A generic response to a request
 *
 *  This response type can store any type of RESP3 data structure.  It
 *  contains the
 *  [pre-order](https://en.wikipedia.org/wiki/Tree_traversal#Pre-order,_NLR)
 *  view of the response tree.
 */
using generic_response = adapter::result<std::vector<resp3::node>>;

struct flat_response_value {
public:
   /// Reserve capacity for nodes and data storage.
   void reserve(std::size_t num_nodes, std::size_t string_size)
   {
      data_.reserve(num_nodes * string_size);
      view_.reserve(num_nodes);
   }

   std::vector<resp3::offset_node> const& view() const { return view_; }
   std::vector<resp3::offset_node>& view() { return view_; }

   template <typename String>
   void push_back(const resp3::basic_node<String>& nd)
   {
      resp3::offset_string offset_string;
      offset_string.offset = data_.size();
      offset_string.size = nd.value.size();

      data_.append(nd.value.data());

      offset_string.data = std::string_view{
         data_.data() + offset_string.offset,
         offset_string.size};

      resp3::offset_node new_node;
      new_node.data_type = nd.data_type;
      new_node.aggregate_size = nd.aggregate_size;
      new_node.depth = nd.depth;
      new_node.value = std::move(offset_string);

      view_.push_back(std::move(new_node));
   }

private:
   std::string data_;
   std::vector<resp3::offset_node> view_;
};

/** @brief A memory-efficient generic response to a request.
 *  @ingroup high-level-api
 * 
 *  Uses a compact buffer to store RESP3 data with reduced allocations.
 */
using generic_flat_response = adapter::result<flat_response_value>;

/** @brief Consume on response from a generic response
 *
 *  This function rotates the elements so that the start of the next
 *  response becomes the new front element. For example the output of
 *  the following code
 *
 * @code
 * request req;
 * req.push("PING", "one");
 * req.push("PING", "two");
 * req.push("PING", "three");
 *
 * generic_response resp;
 * co_await conn.async_exec(req, resp);
 *
 * std::cout << "PING: " << resp.value().front().value << std::endl;
 * consume_one(resp);
 * std::cout << "PING: " << resp.value().front().value << std::endl;
 * consume_one(resp);
 * std::cout << "PING: " << resp.value().front().value << std::endl;
 * @endcode
 *
 * Is:
 *
 * @code
 * PING: one
 * PING: two
 * PING: three
 * @endcode
 *
 * Given that this function rotates elements, it won't be very
 * efficient for responses with a large number of elements. It was
 * introduced mainly to deal with buffers server pushes as shown in
 * the cpp20_subscriber.cpp example. In the future queue-like
 * responses might be introduced to consume in O(1) operations.
 *
 * @param r The response to modify.
 * @param ec Will be populated in case of error.
 */
void consume_one(generic_response& r, system::error_code& ec);

/// Consume on response from a generic flat response
void consume_one(generic_flat_response& r, system::error_code& ec);

/// Throwing overload of `consume_one`.
template <typename Response>
void consume_one(Response& r)
{
   system::error_code ec;
   consume_one(r, ec);
   if (ec)
      throw system::system_error(ec);
}

}  // namespace boost::redis

#endif  // BOOST_REDIS_RESPONSE_HPP
