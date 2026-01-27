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

/// A range of pubsub messages parsed from RESP3 nodes.
///
/// This class provides a range-based interface for iterating over pubsub messages
/// stored in a container of RESP3 nodes (e.g., a @ref resp3::flat_tree).
/// Non-pubsub messages (like subscribe confirmations) are automatically skipped.
///
/// @par Example
/// @code
/// resp3::flat_tree tree;
/// // ... populate tree ...
/// for (const auto& msg : pubsub_messages(tree)) {
///    std::cout << "Channel: " << msg.channel << ", Payload: " << msg.payload << "\n";
/// }
/// @endcode
class pubsub_messages {
   span<const resp3::node_view> nodes_;

public:
   /// The iterator type.
   /// Forward iterator for parsing pubsub messages from a range of RESP3 nodes.
   class iterator {
      const resp3::node_view* current_{};
      const resp3::node_view* end_{};
      pubsub_message msg_{};

      friend class pubsub_messages;

      void advance();

      iterator(const resp3::node_view* first, const resp3::node_view* last) noexcept
      : current_{first}
      , end_{last}
      {
         advance();
      }

   public:
      using value_type = pubsub_message;
      using difference_type = std::ptrdiff_t;
      using pointer = pubsub_message;
      using reference = pubsub_message;
      using iterator_category = std::forward_iterator_tag;

      /// Constructs an end iterator.
      iterator() = default;

      /// Returns a reference to the current pubsub message.
      reference operator*() const noexcept { return msg_; }

      pointer operator->() const noexcept { return msg_; }

      /// Advances to the next pubsub message.
      iterator& operator++() noexcept
      {
         advance();
         return *this;
      }

      /// Advances to the next pubsub message (postfix).
      iterator operator++(int) noexcept
      {
         auto tmp = *this;
         ++*this;
         return tmp;
      }

      /// Compares two iterators for equality.
      friend bool operator==(const iterator& lhs, const iterator& rhs) noexcept
      {
         return lhs.current_ == rhs.current_;
      }

      /// Compares two iterators for inequality.
      friend bool operator!=(const iterator& lhs, const iterator& rhs) noexcept
      {
         return !(lhs == rhs);
      }
   };

   /// Constructs a range from a span of nodes.
   explicit pubsub_messages(span<const resp3::node_view> nodes) noexcept
   : nodes_{nodes}
   { }

   /// Returns an iterator to the first pubsub message.
   iterator begin() const noexcept
   {
      return iterator(nodes_.data(), nodes_.data() + nodes_.size());
   }

   /// Returns an iterator past the last pubsub message.
   iterator end() const noexcept { return iterator(); }
};

}  // namespace boost::redis

#endif
