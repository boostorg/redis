/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/error.hpp>
#include <boost/redis/response.hpp>

#include <boost/assert.hpp>

namespace boost::redis {

namespace {
template <typename Container>
auto& get_value(Container& c)
{
   return c;
}

template <>
auto& get_value(flat_response_value& c)
{
   return c.view();
}

template <typename Response>
void consume_one_impl(Response& r, system::error_code& ec)
{
   if (r.has_error())
      return;  // Nothing to consume.

   auto& value = get_value(r.value());
   if (std::empty(value))
      return;  // Nothing to consume.

   auto const depth = value.front().depth;

   // To simplify we will refuse to consume any data-type that is not
   // a root node. I think there is no use for that and it is complex
   // since it requires updating parent nodes.
   if (depth != 0) {
      ec = error::incompatible_node_depth;
      return;
   }

   auto f = [depth](auto const& e) {
      return e.depth == depth;
   };

   auto match = std::find_if(std::next(std::cbegin(value)), std::cend(value), f);

   value.erase(std::cbegin(value), match);
}

}  // namespace

void consume_one(generic_response& r, system::error_code& ec) { consume_one_impl(r, ec); }

void consume_one(generic_flat_response& r, system::error_code& ec) { consume_one_impl(r, ec); }

void consume_one(generic_response& r)
{
   system::error_code ec;
   consume_one(r, ec);
   if (ec)
      throw system::system_error(ec);
}

void consume_one(generic_flat_response& r)
{
   system::error_code ec;
   consume_one(r, ec);
   if (ec)
      throw system::system_error(ec);
}

}  // namespace boost::redis
