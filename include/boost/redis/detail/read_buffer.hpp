/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_READ_BUFFER_HPP
#define BOOST_REDIS_READ_BUFFER_HPP

#include <boost/core/span.hpp>
#include <boost/system/error_code.hpp>

#include <cstddef>
#include <string_view>
#include <utility>
#include <vector>

namespace boost::redis::detail {

class read_buffer {
public:
   using span_type = span<char>;

   // See config.hpp for the meaning of these parameters.
   struct config {
      std::size_t read_buffer_append_size = 4096u;
      std::size_t max_read_size = static_cast<std::size_t>(-1);
   };

   [[nodiscard]]
   auto prepare_append() -> system::error_code;

   [[nodiscard]]
   auto get_append_buffer() noexcept -> span_type;

   void commit_append(std::size_t read_size);

   [[nodiscard]]
   auto get_committed_buffer() const noexcept -> std::string_view;

   [[nodiscard]]
   auto get_committed_size() const noexcept -> std::size_t;

   void clear();

   // Consume committed data.
   auto consume_committed(std::size_t size) -> std::size_t;

   void reserve(std::size_t n);

   friend bool operator==(read_buffer const& lhs, read_buffer const& rhs);

   friend bool operator!=(read_buffer const& lhs, read_buffer const& rhs);

   void set_config(config const& cfg) noexcept { cfg_ = cfg; };

private:
   config cfg_ = config{};
   std::vector<char> buffer_;
   std::size_t append_buf_begin_ = 0;
};

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_READ_BUFFER_HPP
