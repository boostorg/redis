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
private:
   class iterator {
   public:
      using value_type = resp3::node_view;
      using difference_type = std::ptrdiff_t;
      using pointer = void;
      using reference = value_type;
      using iterator_category = std::forward_iterator_tag;

      explicit iterator(flat_response_value* owner, std::size_t i) noexcept
      : owner_(owner)
      , index_(i)
      { }

      value_type operator*() const { return owner_->operator[](index_); }

      iterator& operator++()
      {
         ++index_;
         return *this;
      }

      bool operator==(const iterator& other) const { return index_ == other.index_; }
      bool operator!=(const iterator& other) const { return !(*this == other); }

   private:
      flat_response_value* owner_;
      std::size_t index_;
   };

   struct offset_node : resp3::node_view {
      std::size_t offset{};
      std::size_t size{};
   };

public:
   resp3::node_view at(std::size_t index) { return make_node_view(view_.at(index)); }

   std::size_t size() { return view_.size(); }

   resp3::node_view operator[](std::size_t index) { return make_node_view(view_[index]); }

   iterator begin() { return iterator{this, 0}; }

   iterator end() { return iterator{this, view_.size()}; }

   template <typename String>
   void push_back(const resp3::basic_node<String>& nd)
   {
      offset_node new_node;
      new_node.data_type = nd.data_type;
      new_node.aggregate_size = nd.aggregate_size;
      new_node.depth = nd.depth;
      new_node.offset = data_.size();
      new_node.size = nd.value.size();

      data_.append(nd.value.data());
      view_.push_back(std::move(new_node));
   }

private:
   resp3::node_view make_node_view(const offset_node& nd)
   {
      resp3::node_view result;
      result.data_type = nd.data_type;
      result.aggregate_size = nd.aggregate_size;
      result.depth = nd.depth;
      result.value = std::string_view{data_.data() + nd.offset, nd.size};
      return result;
   }

   std::string data_;
   std::vector<offset_node> view_;
};

/**
 * TODO: documentation
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

/**
 * @brief Throwing overload of `consume_one`.
 *
 * @param r The response to modify.
 */
void consume_one(generic_response& r);

}  // namespace boost::redis

#endif  // BOOST_REDIS_RESPONSE_HPP
