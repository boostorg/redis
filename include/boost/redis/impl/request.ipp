/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/request.hpp>
#include <boost/redis/resp3/serialization.hpp>
#include <boost/redis/resp3/type.hpp>

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

boost::redis::resp3::command_context boost::redis::request::start_command(
   std::string_view cmd,
   std::size_t num_args)
{
   // Determine the pubsub change type that this command is performing
   // TODO: this has overlap with has_response
   auto change_type = detail::pubsub_change_type::none;
   if (cfg_.pubsub_state_restoration) {
      if (cmd == "SUBSCRIBE")
         change_type = detail::pubsub_change_type::subscribe;
      if (cmd == "PSUBSCRIBE")
         change_type = detail::pubsub_change_type::psubscribe;
      if (cmd == "UNSUBSCRIBE")
         change_type = detail::pubsub_change_type::unsubscribe;
      if (cmd == "PUNSUBSCRIBE")
         change_type = detail::pubsub_change_type::punsubscribe;
   }

   // Add the header
   resp3::add_header(
      payload_,
      resp3::type::array,
      num_args + 1u);  // the command string is also an array member

   // Serialize the command string
   resp3::boost_redis_to_bulk(payload_, cmd);

   // Compose the command context
   return {change_type, pubsub_changes_, payload_};
}
