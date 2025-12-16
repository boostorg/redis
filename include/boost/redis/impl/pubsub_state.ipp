//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/detail/pubsub_state.hpp>
#include <boost/redis/request.hpp>

#include <boost/assert.hpp>

#include <string>

namespace boost::redis::detail {

void pubsub_state::clear()
{
   channels_.clear();
   pchannels_.clear();
}

void pubsub_state::commit_change(pubsub_change_type type, std::string_view channel)
{
   std::string owning_channel{channel};
   switch (type) {
      case pubsub_change_type::subscribe:    channels_.insert(std::move(owning_channel)); break;
      case pubsub_change_type::unsubscribe:  channels_.erase(std::move(owning_channel)); break;
      case pubsub_change_type::psubscribe:   pchannels_.insert(std::move(owning_channel)); break;
      case pubsub_change_type::punsubscribe: pchannels_.erase(std::move(owning_channel)); break;
      default:                               BOOST_ASSERT(false);
   }
}

void pubsub_state::compose_subscribe_request(request& to) const
{
   to.push_range("SUBSCRIBE", channels_);
   to.push_range("PBSUBSCRIBE", pchannels_);
}

}  // namespace boost::redis::detail
