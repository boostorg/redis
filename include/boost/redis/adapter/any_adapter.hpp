/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_ANY_ADAPTER_HPP
#define BOOST_REDIS_ANY_ADAPTER_HPP

#include <boost/redis/adapter/adapt.hpp>
#include <boost/redis/resp3/node.hpp>

#include <boost/system/error_code.hpp>

#include <cstddef>
#include <functional>
#include <string_view>
#include <type_traits>

namespace boost::redis {

/** @brief Parse events that an adapter must support.
 */
enum class parse_event
{
   /// Called before the parser starts processing data
   init,
   /// Called for each and every node of RESP3 data
   node,
   /// Called when done processing a complete RESP3 message
   done
};

/** @brief A type-erased reference to a response.
 *
 *  A type-erased response adapter. It can be executed using @ref connection::async_exec.
 *  Using this type instead of raw response references enables separate compilation.
 *
 *  Given a response object `resp` that can be passed to `async_exec`, the following two
 *  statements have the same effect:
 *
 *  @code
 *      co_await conn.async_exec(req, resp);
 *      co_await conn.async_exec(req, any_adapter(...));
 *  @endcode
 */
using any_adapter = std::function<void(parse_event, resp3::node_view const&, system::error_code&)>;

namespace detail {

template <class T>
auto make_any_adapter(T& resp) -> any_adapter
{
   using namespace boost::redis::adapter;

   return [adapter = boost_redis_adapt(
              resp)](parse_event ev, resp3::node_view const& nd, system::error_code& ec) mutable {
      switch (ev) {
         case parse_event::init: adapter.on_init(); break;
         case parse_event::node: adapter.on_node(nd, ec); break;
         case parse_event::done: adapter.on_done(); break;
      }
   };
}

class any_adapter_wrapper {
public:
   any_adapter_wrapper(any_adapter adapter = {}, std::size_t expected_responses = 0u)
   : adapter_{std::move(adapter)}
   , expected_responses_{expected_responses}
   { }

   void on_init()
   {
      system::error_code ec;
      adapter_(parse_event::init, {}, ec);
   };

   void on_done()
   {
      system::error_code ec;
      adapter_(parse_event::done, {}, ec);
      BOOST_ASSERT(expected_responses_ != 0u);
      expected_responses_ -= 1;
   };

   void on_node(resp3::node_view const& nd, system::error_code& ec)
   {
      adapter_(parse_event::node, nd, ec);
   };

   auto get_remaining_responses() const -> std::size_t { return expected_responses_; }

private:
   any_adapter adapter_;
   std::size_t expected_responses_ = 0;
};

}  // namespace detail

}  // namespace boost::redis

#endif
