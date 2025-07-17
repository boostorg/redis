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

system::error_code read_buffer::prepare_append()
{
   BOOST_ASSERT(append_buf_begin_ == buffer_.size());

   auto const new_size = append_buf_begin_ + cfg_.read_buffer_append_size;

   if (new_size > cfg_.max_read_size) {
      return error::exceeds_maximum_read_buffer_size;
   }

   buffer_.resize(new_size);
   return {};
}

void read_buffer::commit_append(std::size_t read_size)
{
   BOOST_ASSERT(buffer_.size() >= (append_buf_begin_ + read_size));
   buffer_.resize(append_buf_begin_ + read_size);
   append_buf_begin_ = buffer_.size();
}

auto read_buffer::get_append_buffer() noexcept -> span_type
{
   auto const size = buffer_.size();
   return make_span(buffer_.data() + append_buf_begin_, size - append_buf_begin_);
}

auto read_buffer::get_committed_buffer() const noexcept -> std::string_view
{
   BOOST_ASSERT(!buffer_.empty());
   return {buffer_.data(), append_buf_begin_};
}

auto read_buffer::get_committed_size() const noexcept -> std::size_t { return append_buf_begin_; }

void read_buffer::clear()
{
   buffer_.clear();
   append_buf_begin_ = 0;
}

std::size_t read_buffer::consume_committed(std::size_t size)
{
   // For convenience, if the requested size is larger than the
   // committed buffer we cap it to the maximum.
   if (size > append_buf_begin_)
      size = append_buf_begin_;

   buffer_.erase(buffer_.begin(), buffer_.begin() + size);
   BOOST_ASSERT(append_buf_begin_ >= size);
   append_buf_begin_ -= size;
   return size;
}

void read_buffer::reserve(std::size_t n) { buffer_.reserve(n); }

bool operator==(read_buffer const& lhs, read_buffer const& rhs)
{
   return lhs.buffer_ == rhs.buffer_ && lhs.append_buf_begin_ == rhs.append_buf_begin_;
}

bool operator!=(read_buffer const& lhs, read_buffer const& rhs) { return !(lhs == rhs); }

}  // namespace boost::redis::detail
