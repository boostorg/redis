//
// Copyright (c) 2026 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_PUSH_PARSER_HPP
#define BOOST_REDIS_PUSH_PARSER_HPP

#include <boost/redis/resp3/node.hpp>

#include <boost/core/span.hpp>

#include <iterator>
#include <string_view>

namespace boost::redis {

/// A push message parsed from a RESP3 push (e.g. pubsub message/pmessage).
struct push_view {
   /// The channel where the message was published.
   std::string_view channel;

   /// The pattern that matched the channel (only for pmessage, empty otherwise).
   std::string_view pattern;

   /// The message payload.
   std::string_view payload;
};

/// Parses push messages from a sequence of RESP3 nodes.
///
/// Non-pubsub messages (e.g. subscribe confirmations) are skipped.
/// The parser must remain alive for the duration of iteration.
///
/// @par Example
/// @code
/// for (const push_view& msg : push_parser(tree)) {
///    std::cout << "Channel: " << msg.channel << ", Payload: " << msg.payload << "\n";
/// }
/// @endcode
class push_parser {
   const resp3::node_view* first_{};
   const resp3::node_view* last_{};
   push_view current_{};
   bool done_{false};

   void advance() noexcept;

public:
   class iterator {
      push_parser* gen_{};

      friend class push_parser;

      explicit iterator(push_parser* gen) noexcept
      : gen_{gen}
      { }

   public:
      using value_type = push_view;
      using difference_type = std::ptrdiff_t;
      using reference = push_view const&;
      using pointer = push_view const*;
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

   explicit push_parser(span<const resp3::node_view> nodes) noexcept
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
