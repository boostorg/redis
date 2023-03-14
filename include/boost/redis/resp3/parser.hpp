/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_RESP3_PARSER_HPP
#define BOOST_REDIS_RESP3_PARSER_HPP

#include <boost/redis/resp3/node.hpp>
#include <boost/system/error_code.hpp>
#include <array>
#include <limits>
#include <string_view>
#include <cstdint>

namespace boost::redis::resp3 {

using int_type = std::uint64_t;

class parser {
private:
   using node_type = basic_node<std::string_view>;
   static constexpr std::size_t max_embedded_depth = 5;

   // The current depth. Simple data types will have depth 0, whereas
   // the elements of aggregates will have depth 1. Embedded types
   // will have increasing depth.
   std::size_t depth_ = 0;

   // The parser supports up to 5 levels of nested structures. The
   // first element in the sizes stack is a sentinel and must be
   // different from 1.
   std::array<std::size_t, max_embedded_depth + 1> sizes_ = {{1}};

   // Contains the length expected in the next bulk read.
   int_type bulk_length_ = (std::numeric_limits<unsigned long>::max)();

   // The type of the next bulk. Contains type::invalid if no bulk is
   // expected.
   type bulk_ = type::invalid;

public:
   parser();

   // Returns the number of bytes that have been consumed.
   auto
   consume(
      char const* data,
      std::size_t n,
      system::error_code& ec) -> std::pair<node_type, std::size_t>;

   // Returns true when the parser is done with the current message.
   [[nodiscard]] auto done() const noexcept
      { return depth_ == 0 && bulk_ == type::invalid; }

   // The bulk type expected in the next read. If none is expected
   // returns type::invalid.
   [[nodiscard]] auto bulk_expected() const noexcept -> bool
      { return bulk_ != type::invalid; }

   // The length expected in the the next bulk.
   [[nodiscard]] auto bulk_length() const noexcept
      { return bulk_length_; }
};

} // boost::redis::resp3

#endif // BOOST_REDIS_RESP3_PARSER_HPP
