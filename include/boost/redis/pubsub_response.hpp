/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_PUBSUB_RESPONSE_HPP
#define BOOST_REDIS_PUBSUB_RESPONSE_HPP

#include <boost/redis/adapter/detail/adapters.hpp>
#include <boost/redis/adapter/detail/response_traits.hpp>
#include <boost/redis/detail/coroutine.hpp>
#include <boost/redis/resp3/node.hpp>
#include <boost/redis/resp3/type.hpp>

#include <boost/core/span.hpp>
#include <boost/system/detail/error_code.hpp>
#include <boost/system/error_code.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace boost::redis {

template <class T = std::string>
struct pubsub_message {
   std::string channel;
   T payload;
};

namespace adapter::detail {

template <class T>
struct pubsub_adapter {
   std::vector<pubsub_message<T>>* vec;
   int resume_point{0};
   bool ignore{false};
   std::string channel_name{};

   void on_init()
   {
      resume_point = 0;
      ignore = false;
   }
   void on_done() { }

   system::error_code on_node_impl(resp3::node_view const& nd)
   {
      if (ignore)
         return system::error_code();

      switch (resume_point) {
         BOOST_REDIS_CORO_INITIAL

         // The root node should be a push
         if (nd.data_type != resp3::type::push) {
            ignore = true;
            return system::error_code();
         }

         BOOST_REDIS_YIELD(resume_point, 1, system::error_code())

         // The next element should be the string "message"
         if (nd.data_type != resp3::type::blob_string || nd.value != "message") {
            ignore = true;
            return system::error_code();
         }

         BOOST_REDIS_YIELD(resume_point, 2, system::error_code())

         // The next element is the channel name
         if (nd.data_type != resp3::type::blob_string) {
            ignore = true;
            return system::error_code();
         }

         channel_name = nd.value;

         BOOST_REDIS_YIELD(resume_point, 3, system::error_code())

         // The last element is the payload
         if (nd.data_type != resp3::type::blob_string) {
            ignore = true;
            return system::error_code();
         }

         // Try to deserialize the payload, if required
         T elm;
         system::error_code ec;
         boost_redis_from_bulk(elm, nd, ec);

         if (ec)
            return ec;

         vec->push_back(pubsub_message<T>{std::move(channel_name), std::move(elm)});

         return system::error_code();
      }

      return system::error_code();
   }

   template <class String>
   void on_node(resp3::basic_node<String> const& nd, system::error_code& ec)
   {
      ec = on_node_impl(nd);
   }
};

template <class T>
struct response_traits<std::vector<pubsub_message<T>>> {
   using response_type = std::vector<pubsub_message<T>>;
   using adapter_type = pubsub_adapter<T>;

   static auto adapt(response_type& v) noexcept { return adapter_type{&v}; }
};

}  // namespace adapter::detail

}  // namespace boost::redis

#endif
