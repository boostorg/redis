//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_PUBSUB_STATE_HPP
#define BOOST_REDIS_PUBSUB_STATE_HPP

#include <string_view>

namespace boost::redis {

class request;

namespace detail {

enum class pubsub_change_type;

class pubsub_state {
public:
   pubsub_state() = default;
   void clear();
   void commit_change(pubsub_change_type type, std::string_view channel);
   void compose_subscribe_request(request& to) const;
};

}  // namespace detail
}  // namespace boost::redis

#endif
