//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_UPDATE_SENTINEL_LIST_HPP
#define BOOST_REDIS_UPDATE_SENTINEL_LIST_HPP

#include <boost/redis/config.hpp>

#include <boost/core/span.hpp>

#include <cstddef>
#include <utility>
#include <vector>

namespace boost::redis::detail {

// Updates the internal Sentinel list.
// to should never be empty
inline void update_sentinel_list(
   std::vector<address>& to,
   std::size_t current_index,               // the one to maintain and place first
   span<const address> gossip_sentinels,    // the ones that SENTINEL SENTINELS returned
   span<const address> bootstrap_sentinels  // the ones the user supplied
)
{
   BOOST_ASSERT(!to.empty());

   // Place the one that succeeded in the front
   if (current_index != 0u)
      std::swap(to.front(), to[current_index]);

   // Remove the other Sentinels
   to.resize(1u);

   // Add one group
   to.insert(to.end(), gossip_sentinels.begin(), gossip_sentinels.end());

   // Insert any user-supplied sentinels, if not already present.
   // This is O(n^2), but is okay because n will be small.
   // Using a sorted vector implies puring boost/container/flat_set.hpp into public headers
   for (const auto& sentinel : bootstrap_sentinels) {
      if (std::find(to.begin(), to.end(), sentinel) == to.end())
         to.push_back(sentinel);
   }
}

}  // namespace boost::redis::detail

#endif
