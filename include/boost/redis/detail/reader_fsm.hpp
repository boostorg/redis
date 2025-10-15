/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_READER_FSM_HPP
#define BOOST_REDIS_READER_FSM_HPP

#include <boost/redis/detail/connection_state.hpp>
#include <boost/redis/detail/multiplexer.hpp>

#include <boost/asio/cancellation_type.hpp>
#include <boost/system/error_code.hpp>

#include <cstddef>

namespace boost::redis::detail {

class read_buffer;

class reader_fsm {
public:
   struct action {
      enum class type
      {
         read_some,
         needs_more,
         notify_push_receiver,
         done,
      };

      action(type t, std::size_t push_size = 0u) noexcept
      : type_(t)
      , push_size_(push_size)
      { }

      action(system::error_code ec) noexcept
      : type_(type::done)
      , ec_(ec)
      { }

      static action notify_push_receiver(std::size_t bytes)
      {
         return {type::notify_push_receiver, bytes};
      }

      type type_;
      std::size_t push_size_{};
      system::error_code ec_;
   };

   action resume(
      connection_state& st,
      std::size_t bytes_read,
      system::error_code ec,
      asio::cancellation_type_t cancel_state);

   reader_fsm() = default;

private:
   int resume_point_{0};
   action::type next_read_type_ = action::type::read_some;
   std::pair<consume_result, std::size_t> res_{consume_result::needs_more, 0u};
};

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_READER_FSM_HPP
