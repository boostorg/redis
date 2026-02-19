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
/// The current message is cached; use advance(), done(), and current(),
/// or the range interface. The generator must remain alive for the duration of iteration.
///
/// @par Example (advance / done / current)
/// @code
/// pubsub_generator gen(tree);
/// while (!gen.done()) {
///    std::cout << "Channel: " << gen.current().channel << ", Payload: " << gen.current().payload << "\n";
///    gen.advance();
/// }
/// @endcode
///
/// @par Example (range)
/// @code
/// pubsub_generator gen(tree);
/// for (const pubsub_message& msg : gen) {
///    std::cout << "Channel: " << msg.channel << ", Payload: " << msg.payload << "\n";
/// }
/// @endcode
class pubsub_generator {
   const resp3::node_view* current_{};
   const resp3::node_view* end_{};
   pubsub_message cached_{};
   bool done_{false};

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

      reference operator*() const noexcept { return gen_->current(); }
      pointer operator->() const noexcept { return &gen_->current(); }

      iterator& operator++() noexcept
      {
         BOOST_ASSERT(gen_);
         gen_->advance();
         if (gen_->done())
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
   : current_{nodes.data()}
   , end_{nodes.data() + nodes.size()}
   {
      advance();
   }

   /// Advances to the next pubsub message (or to done if none left).
   void advance() noexcept;

   /// True when there is no current message (before first or after last).
   bool done() const noexcept { return done_; }

   /// Current message. Undefined if done().
   pubsub_message const& current() const noexcept { return cached_; }

   iterator begin() noexcept { return iterator(this); }
   iterator end() noexcept { return iterator(); }
};

}  // namespace boost::redis

#endif
