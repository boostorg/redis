/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_READ_BUFFER_HPP
#define BOOST_REDIS_READ_BUFFER_HPP

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace boost::redis::detail {

class read_buffer {
public:
   void prepare_append(
      std::size_t append_size,
      std::size_t max_buffer_size,
      system::error_code& ec);

   void commit_append(std::size_t read_size);

   [[nodiscard]]
   auto get_append_buffer() noexcept -> std::pair<char*, std::size_t>;

   [[nodiscard]]
   auto get_committed_buffer() const noexcept -> std::string_view;

   [[nodiscard]]
   auto get_committed_size() const noexcept -> std::size_t;

   void clear();

   void consume(std::size_t size);

private:
   std::string buffer_;
   std::size_t append_buf_begin_ = 0;
};

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_READ_BUFFER_HPP
