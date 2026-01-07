/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/request.hpp>
#include <boost/redis/resp3/serialization.hpp>

#include <cstddef>
#include <string_view>

namespace boost::redis::detail {

auto has_response(std::string_view cmd) -> bool
{
   if (cmd == "SUBSCRIBE")
      return true;
   if (cmd == "PSUBSCRIBE")
      return true;
   if (cmd == "UNSUBSCRIBE")
      return true;
   if (cmd == "PUNSUBSCRIBE")
      return true;
   return false;
}

request make_hello_request()
{
   request req;
   req.push("HELLO", "3");
   return req;
}

}  // namespace boost::redis::detail

void boost::redis::request::append(const request& other)
{
   payload_ += other.payload_;
   commands_ += other.commands_;
   expected_responses_ += other.expected_responses_;
}

void boost::redis::request::add_pubsub_arg(detail::pubsub_change_type type, std::string_view value)
{
   // Add the argument
   resp3::add_bulk(payload_, value);

   // Track the change.
   // The final \r\n adds 2 bytes
   std::size_t offset = payload_.size() - value.size() - 2u;
   pubsub_changes_.push_back({type, offset, value.size()});
}
