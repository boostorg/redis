//
// Copyright (c) 2026 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_PUBSUB_MESSAGES_HPP
#define BOOST_REDIS_PUBSUB_MESSAGES_HPP

#include <boost/redis/resp3/node.hpp>

#include <boost/core/span.hpp>

#include <iterator>
#include <string_view>

namespace boost::redis {

/// A pubsub message parsed from a RESP3 push.
struct pubsub_message {
   /// The channel where the message was published.
   std::string_view channel;

   /// The pattern that matched the channel (only for pmessage, empty otherwise).
   std::string_view pattern;

   /// The message payload.
   std::string_view payload;
};

/// Parses pubsub messages from a sequence of RESP3 nodes.
///
/// Non-pubsub messages (e.g. subscribe confirmations) are skipped.
/// The generator must remain alive for the duration of iteration.
///
/// @par Example
/// @code
/// for (const pubsub_message& msg : pubsub_generator(tree)) {
///    std::cout << "Channel: " << msg.channel << ", Payload: " << msg.payload << "\n";
/// }
/// @endcode
class pubsub_generator {
   const resp3::node_view* first_{};
   const resp3::node_view* last_{};
   pubsub_message current_{};
   bool done_{false};

   void advance() noexcept;

public:
   class iterator {
      pubsub_generator* gen_{};

      friend class pubsub_generator;

      explicit iterator(pubsub_generator* gen) noexcept
      : gen_{gen}
      { }

   public:
      using value_type = pubsub_message;
      using difference_type = std::ptrdiff_t;
      using reference = pubsub_message const&;
      using pointer = pubsub_message const*;
      using iterator_category = std::input_iterator_tag;

      iterator() = default;

      reference operator*() const noexcept { return gen_->current_; }
      pointer operator->() const noexcept { return &gen_->current_; }

      iterator& operator++() noexcept
      {
         BOOST_ASSERT(gen_);
         gen_->advance();
         if (gen_->done_)
            gen_ = nullptr;
         return *this;
      }

      void operator++(int) noexcept { ++*this; }

      friend bool operator==(const iterator& a, const iterator& b) noexcept
      {
         return a.gen_ == b.gen_;
      }

      friend bool operator!=(const iterator& a, const iterator& b) noexcept { return !(a == b); }
   };

   explicit pubsub_generator(span<const resp3::node_view> nodes) noexcept
   : first_{nodes.data()}
   , last_{nodes.data() + nodes.size()}
   {
      advance();
   }

   iterator begin() noexcept { return iterator(this); }
   iterator end() noexcept { return iterator(); }
};

}  // namespace boost::redis

#endif
