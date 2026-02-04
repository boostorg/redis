/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/detail/read_buffer.hpp>

#include <boost/assert.hpp>
#include <boost/core/make_span.hpp>

#include <utility>

namespace boost::redis::detail {

system::error_code read_buffer::prepare()
{
   BOOST_ASSERT(prepared_begin_ == buffer_.size());

   auto const new_size = prepared_begin_ + cfg_.read_buffer_append_size;

   if (new_size > cfg_.max_read_size) {
      return error::exceeds_maximum_read_buffer_size;
   }

   buffer_.resize(new_size);
   return {};
}

void read_buffer::commit(std::size_t read_size)
{
   BOOST_ASSERT(buffer_.size() >= (prepared_begin_ + read_size));
   buffer_.resize(prepared_begin_ + read_size);
   prepared_begin_ = buffer_.size();
}

auto read_buffer::get_prepared() noexcept -> span_type
{
   auto const size = buffer_.size();
   return make_span(buffer_.data() + prepared_begin_, size - prepared_begin_);
}

auto read_buffer::get_commited() const noexcept -> std::string_view
{
   return {buffer_.data() + consumed_, prepared_begin_ - consumed_};
}

void read_buffer::clear()
{
   buffer_.clear();
   consumed_ = 0;
   prepared_begin_ = 0;
}

read_buffer::consume_result
read_buffer::consume(std::size_t size)
{
   // For convenience, if the requested size is larger than the
   // committed buffer we cap it to the maximum.
   auto const consumable = prepared_begin_ - consumed_;
   if (size > consumable)
      size = consumable;

   consumed_ += size;
   BOOST_ASSERT(consumed_ <= prepared_begin_);

   auto rotated = 0u;
   if (consumed_ >= 0 && size > 0u) {
      buffer_.erase(buffer_.begin(), buffer_.begin() + consumed_);
      rotated = buffer_.size();

      BOOST_ASSERT(consumed_ <= prepared_begin_);
      prepared_begin_ -= consumed_;
      consumed_ = 0u;
   }

   return {size, rotated};
}

void read_buffer::reserve(std::size_t n) { buffer_.reserve(n); }

bool operator==(read_buffer const& lhs, read_buffer const& rhs)
{
   return lhs.buffer_ == rhs.buffer_ && lhs.prepared_begin_ == rhs.prepared_begin_;
}

bool operator!=(read_buffer const& lhs, read_buffer const& rhs) { return !(lhs == rhs); }

}  // namespace boost::redis::detail
