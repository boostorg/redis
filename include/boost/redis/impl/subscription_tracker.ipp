//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/detail/subscription_tracker.hpp>
#include <boost/redis/request.hpp>

#include <boost/assert.hpp>

#include <string>

namespace boost::redis::detail {

// Given a request and a change, returns an owning string
// with the channel or pattern name affected by the change
inline std::string get_channel_owning(const request& req, const pubsub_change& ch)
{
   return std::string(req.payload().substr(ch.channel_offset, ch.channel_size));
}

void subscription_tracker::clear()
{
   channels_.clear();
   pchannels_.clear();
}

void subscription_tracker::commit_changes(const request& req)
{
   for (const auto& ch : request_access::pubsub_changes(req)) {
      switch (ch.type) {
         case pubsub_change_type::subscribe:   channels_.insert(get_channel_owning(req, ch)); break;
         case pubsub_change_type::unsubscribe: channels_.erase(get_channel_owning(req, ch)); break;
         case pubsub_change_type::psubscribe:  pchannels_.insert(get_channel_owning(req, ch)); break;
         case pubsub_change_type::punsubscribe:
            pchannels_.erase(get_channel_owning(req, ch));
            break;
         case pubsub_change_type::unsubscribe_all:  channels_.clear(); break;
         case pubsub_change_type::punsubscribe_all: pchannels_.clear(); break;
         default:                                   BOOST_ASSERT(false);
      }
   }
}

void subscription_tracker::compose_subscribe_request(request& to) const
{
   to.push_range("SUBSCRIBE", channels_);
   to.push_range("PSUBSCRIBE", pchannels_);
}

}  // namespace boost::redis::detail
