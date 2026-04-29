/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_DETAIL_FLOW_CONTROLLER_HPP
#define BOOST_REDIS_DETAIL_FLOW_CONTROLLER_HPP

#include <boost/assert.hpp>
#include <boost/capy/error.hpp>
#include <boost/capy/ex/async_event.hpp>
#include <boost/capy/ex/this_coro.hpp>
#include <boost/capy/io_task.hpp>

#include <cstddef>

namespace boost::redis::detail {

// Allows controlling the received pushes. This is a substitute
// for the old channel-based implementation.
// The object stores the number of pending bytes.
// The receiver calls take(), which marks the object as empty. take() blocks if no bytes are pending.
// The reader calls try_put() and put(), which increases the pending byte count.
// When the pending byte count raises above the configured max bytes, put() blocks.
// The actual pending bytes may temporarily exceed the configured max bytes, but subsequent put() calls will block.
class flow_controller {
   std::size_t pending_bytes_{};
   std::size_t max_bytes_;
   capy::async_event bytes_available_;
   capy::async_event room_available_;

public:
   flow_controller(std::size_t max_bytes)
   : max_bytes_(max_bytes)
   {
      room_available_.set();
      BOOST_ASSERT(max_bytes != 0u);
   }

   // Waits until at least one byte has been put in the flow controller.
   capy::io_task<> take()
   {
      while (pending_bytes_ == 0u) {
         auto [ec] = co_await bytes_available_.wait();
         if (ec)
            co_return {ec};
      }
      pending_bytes_ = 0u;
      bytes_available_.clear();
      room_available_.set();
      co_return {};
   }

   // Tries to put bytes into the object, without blocking.
   // Returns true if the operation succeeded.
   // Otherwise, does nothing and returns false.
   bool try_put(std::size_t bytes)
   {
      // Do we have space?
      if (!room_available_.is_set())
         return false;

      // Add the bytes. We might surpass the limit slightly, but this is OK
      // because we've already read the bytes. This avoids problems in the theoretical
      // case of reading a very big push.
      // The following messages will wait
      pending_bytes_ += bytes;
      if (pending_bytes_ >= max_bytes_)
         room_available_.clear();
      bytes_available_.set();

      return true;
   }

   // Puts bytes into the object. Blocks if the object is full.
   capy::io_task<> put(std::size_t bytes)
   {
      while (!try_put(bytes)) {
         auto [ec] = co_await room_available_.wait();
         if (ec)
            co_return {ec};
      }
      co_return {};
   }

   // Exposed for testing
   std::size_t pending_bytes() const { return pending_bytes_; }
};

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_FLOW_CONTROLLER_HPP
