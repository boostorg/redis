/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_RESP3_NODE_HPP
#define AEDIS_RESP3_NODE_HPP

#include <aedis/resp3/type.hpp>

namespace aedis::resp3 {

/** \brief A node in the response tree.
 *  \ingroup high-level-api
 *
 *  RESP3 can contain recursive data structures: A map of sets of
 *  vector of etc. As it is parsed each element is passed to user
 *  callbacks (push parser), the `aedis::adapt` function. The signature of this
 *  callback is `f(resp3::node<std::string_view)`. This class is called a node
 *  because it can be seen as the element of the response tree. It
 *  is a template so that users can use it with owing strings e.g.
 *  `std::string` or `boost::static_string` etc. if they decide to use a node as
 *  response type, for example, to read a non-aggregate data-type use
 *
 *  ```cpp
 *  resp3::node<std::string> resp;
 *  co_await conn->async_exec(req, adapt(resp));
 *  ```
 *
 *  for an aggregate use instead
 *
 *  ```cpp
 *  std::vector<resp3::node<std::string>> resp; co_await
 *  conn->async_exec(req, adapt(resp));
 *  ```
 *
 *  The vector will contain the
 *  [pre-order](https://en.wikipedia.org/wiki/Tree_traversal#Pre-order,_NLR)
 *  view of the response tree.  Any Redis response can be received in
 *  an array of nodes as shown above.
 *
 *  \tparam String A `std::string`-like type.
 */
template <class String>
struct node {
   /// The RESP3 type of the data in this node.
   type data_type = type::invalid;

   /// The number of elements of an aggregate.
   std::size_t aggregate_size{};

   /// The depth of this node in the response tree.
   std::size_t depth{};

   /// The actual data. For aggregate types this is usually empty.
   String value{};
};

/** @brief Compares a node for equality.
 *  @relates node
 *
 *  @param a Left hand side node object.
 *  @param b Right hand side node object.
 */
template <class String>
auto operator==(node<String> const& a, node<String> const& b)
{
   return a.aggregate_size == b.aggregate_size
       && a.depth == b.depth
       && a.data_type == b.data_type
       && a.value == b.value;
};

} // aedis::resp3

#endif // AEDIS_RESP3_NODE_HPP
