/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_SEMAPHORE_HPP
#define BOOST_REDIS_SEMAPHORE_HPP

#include <boost/capy/error.hpp>
#include <boost/capy/ex/async_event.hpp>
#include <boost/capy/ex/this_coro.hpp>
#include <boost/capy/io_task.hpp>

#include <cassert>
#include <cstddef>

namespace boost::redis::detail {

class semaphore {
   std::size_t pending_bytes_{};
   std::size_t max_bytes_;
   capy::async_event bytes_available_;
   capy::async_event room_available_;

public:
   semaphore(std::size_t max_bytes) noexcept
   : max_bytes_(max_bytes)
   {
      assert(max_bytes != 0u);
   }

   /** Waits until at least one byte has been put in the semaphore. */
   capy::io_task<> take()
   {
      while (pending_bytes_ == 0u) {
         // TODO: update this when cancellation is implemented for events
         if ((co_await capy::this_coro::stop_token).stop_requested())
            co_return {capy::error::canceled};
         co_await bytes_available_.wait();
      }
      pending_bytes_ = 0u;
      bytes_available_.clear();
      room_available_.set();
      co_return {};
   }

   capy::io_task<> wait_for_space()
   {
      while (pending_bytes_ >= max_bytes_) {
         // TODO: update this when cancellation is implemented for events
         if ((co_await capy::this_coro::stop_token).stop_requested())
            co_return {capy::error::canceled};
         co_await room_available_.wait();
      }
      co_return {};
   }

   void put(std::size_t bytes)
   {
      pending_bytes_ += bytes;
      if (pending_bytes_ >= max_bytes_)
         room_available_.clear();
      bytes_available_.set();
   }
};

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_SEMAPHORE_HPP
