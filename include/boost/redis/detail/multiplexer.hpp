/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_MULTIPLEXER_HPP
#define BOOST_REDIS_MULTIPLEXER_HPP

#include <boost/redis/adapter/adapt.hpp>
#include <boost/redis/adapter/any_adapter.hpp>
#include <boost/redis/config.hpp>
#include <boost/redis/detail/read_buffer.hpp>
#include <boost/redis/resp3/node.hpp>
#include <boost/redis/resp3/parser.hpp>
#include <boost/redis/resp3/type.hpp>
#include <boost/redis/usage.hpp>

#include <boost/system/error_code.hpp>

#include <deque>
#include <functional>
#include <memory>
#include <string_view>
#include <utility>

namespace boost::redis {

class request;

namespace detail {

// Return type of the multiplexer::consume_next function
enum class consume_result
{
   needs_more,    // consume_next didn't have enough data
   got_response,  // got a response to a regular command, vs. a push
   got_push,      // got a response to a push
};

class multiplexer {
public:
   struct elem {
   public:
      explicit elem(request const& req, any_adapter adapter);

      void set_done_callback(std::function<void()> f) noexcept { done_ = std::move(f); };

      auto notify_done() noexcept -> void
      {
         status_ = status::done;
         done_();
      }

      auto notify_error(system::error_code ec) noexcept -> void;

      [[nodiscard]]
      auto is_waiting() const noexcept
      {
         return status_ == status::waiting;
      }

      [[nodiscard]]
      auto is_written() const noexcept
      {
         return status_ == status::written;
      }

      [[nodiscard]]
      auto is_staged() const noexcept
      {
         return status_ == status::staged;
      }

      [[nodiscard]]
      bool is_done() const noexcept
      {
         return status_ == status::done;
      }

      void mark_written() noexcept { status_ = status::written; }

      void mark_staged() noexcept { status_ = status::staged; }

      void mark_waiting() noexcept { status_ = status::waiting; }

      auto get_error() const -> system::error_code const& { return ec_; }

      auto get_request() const -> request const& { return *req_; }

      auto get_read_size() const -> std::size_t { return read_size_; }

      auto get_remaining_responses() const -> std::size_t { return remaining_responses_; }

      auto commit_response(std::size_t read_size) -> void;

      auto get_adapter() -> any_adapter& { return adapter_; }

      // Marks the element as an abandoned request. An abandoned request
      // won't cause problems when its response arrives, but that response will be ignored.
      void mark_abandoned();

      [[nodiscard]]
      bool is_abandoned() const
      {
         return !req_;
      }

   private:
      enum class status
      {
         waiting,  // the request hasn't been written yet
         staged,   // we've issued the write for this request, but it hasn't finished yet
         written,  // the request has been written successfully
         done,     // the request has completed and the done callback has been invoked
      };

      request const* req_;
      any_adapter adapter_;
      std::function<void()> done_;

      // Contains the number of commands that haven't been read yet.
      std::size_t remaining_responses_;
      status status_;

      system::error_code ec_;
      std::size_t read_size_;
   };

   multiplexer();

   // To be called before a write operation. Coalesces all available requests
   // into a single buffer. Returns the number of coalesced requests.
   // Must be called before cancel_on_conn_lost() because it might change
   // request status.
   [[nodiscard]]
   auto prepare_write() -> std::size_t;

   // To be called after a successful write operation.
   // Returns the number of requests that have been released because
   // they don't have a response e.g. SUBSCRIBE.
   // Must be called before cancel_on_conn_lost() because it might change
   // request status.
   auto commit_write() -> std::size_t;

   // To be called after a successful read operation.
   // Must be called before cancel_on_conn_lost() because it might change
   // request status.
   [[nodiscard]]
   auto consume(system::error_code& ec) -> std::pair<consume_result, std::size_t>;

   auto add(std::shared_ptr<elem> const& ptr) -> void;
   void cancel(std::shared_ptr<elem> const& ptr);
   auto reset() -> void;

   [[nodiscard]]
   auto const& get_parser() const noexcept
   {
      return parser_;
   }

   auto cancel_waiting() -> std::size_t;

   // To be called exactly once to clean up state after a connection becomes unhealthy.
   // Requests are canceled or returned to the waiting state to be re-sent to the server,
   // depending on their configuration. After this function is called, prepare_write,
   // commit_write and consume_next must not be called until a reset() happens.
   // Otherwise, race conditions like the following might happen
   // (see https://github.com/boostorg/redis/pull/309 and https://github.com/boostorg/redis/issues/181):
   //
   //   - This function runs and cancels a request, then consume_next runs. It tries to access
   //     a request and adapter that might have been destroyed.
   //   - This function runs and returns a request to waiting, then prepare_write runs.
   //     It incorrectly sets the request state to staged, causing de synchronization between requests and responses.
   void cancel_on_conn_lost();

   [[nodiscard]]
   auto get_write_buffer() const noexcept -> std::string_view
   {
      return std::string_view{write_buffer_};
   }

   [[nodiscard]]
   auto get_prepared_read_buffer() noexcept -> read_buffer::span_type;

   [[nodiscard]]
   auto prepare_read() noexcept -> system::error_code;

   void commit_read(std::size_t read_size);

   [[nodiscard]]
   auto get_read_buffer_size() const noexcept -> std::size_t;

   void set_receive_adapter(any_adapter adapter);

   [[nodiscard]]
   auto get_usage() const noexcept -> usage
   {
      return usage_;
   }

   [[nodiscard]]
   auto is_writing() const noexcept -> bool;

   void set_config(config const& cfg);

private:
   void commit_usage(bool is_push, read_buffer::consume_result res);

   [[nodiscard]]
   auto is_next_push(std::string_view data) const noexcept -> bool;

   // Releases the number of requests that have been released.
   [[nodiscard]]
   auto release_push_requests() -> std::size_t;

   [[nodiscard]]
   consume_result consume_impl(system::error_code& ec);

   read_buffer read_buffer_;
   std::string write_buffer_;
   std::deque<std::shared_ptr<elem>> reqs_;
   resp3::parser parser_{};
   bool on_push_ = false;
   bool cancel_run_called_ = false;
   usage usage_;
   any_adapter receive_adapter_;
};

auto make_elem(request const& req, any_adapter adapter) -> std::shared_ptr<multiplexer::elem>;

}  // namespace detail
}  // namespace boost::redis

#endif  // BOOST_REDIS_MULTIPLEXER_HPP
