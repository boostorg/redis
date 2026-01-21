//
// Copyright (c) 2026 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/resp3/messages_view.hpp>
#include <boost/redis/resp3/node.hpp>

#include <algorithm>
#include <cstddef>

namespace boost::redis::resp3 {

std::size_t messages_view::compute_message_size(span<const node_view> messages)
{
   if (messages.empty())
      return 0u;
   auto it = std::find_if(messages.begin() + 1u, messages.end(), [](const resp3::node_view& n) {
      return n.depth == 0u;
   });
   return it - messages.begin();
}

}  // namespace boost::redis::resp3
