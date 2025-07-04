/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/detail/read_buffer.hpp>

#include <boost/assert.hpp>

#include <utility>

namespace boost::redis::detail {

void read_buffer::prepare_append(
   std::size_t append_size,
   std::size_t max_buffer_size,
   system::error_code& ec)
{
   BOOST_ASSERT(append_buf_begin_ == buffer_.size());

   auto const new_size = append_buf_begin_ + append_size;

   if (new_size > max_buffer_size) {
      ec = error::exceeds_maximum_read_buffer_size;
      return;
   }

   buffer_.resize(new_size);
}

void read_buffer::commit_append(std::size_t read_size)
{
   BOOST_ASSERT(buffer_.size() >= (append_buf_begin_ + read_size));
   buffer_.resize(append_buf_begin_ + read_size);
   append_buf_begin_ = buffer_.size();
}

auto read_buffer::get_append_buffer() noexcept -> std::pair<char*, std::size_t>
{
   auto const size = buffer_.size();
   return std::make_pair(buffer_.data() + append_buf_begin_, size - append_buf_begin_);
}

auto read_buffer::get_committed_buffer() const noexcept -> std::string_view
{
   return {buffer_.data(), append_buf_begin_};
}

auto read_buffer::get_committed_size() const noexcept -> std::size_t { return append_buf_begin_; }

void read_buffer::clear()
{
   buffer_.clear();
   append_buf_begin_ = 0;
}

void read_buffer::consume(std::size_t size)
{
   buffer_.erase(0, size);
   BOOST_ASSERT(append_buf_begin_ >= size);
   append_buf_begin_ -= size;
}

}  // namespace boost::redis::detail
