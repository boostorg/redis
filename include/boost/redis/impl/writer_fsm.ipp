//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_WRITER_FSM_IPP
#define BOOST_REDIS_WRITER_FSM_IPP

#include <boost/redis/adapter/any_adapter.hpp>
#include <boost/redis/detail/connection_logger.hpp>
#include <boost/redis/detail/connection_state.hpp>
#include <boost/redis/detail/coroutine.hpp>
#include <boost/redis/detail/multiplexer.hpp>
#include <boost/redis/detail/writer_fsm.hpp>
#include <boost/redis/impl/is_terminal_cancel.hpp>

#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/error.hpp>
#include <boost/assert.hpp>
#include <boost/system/error_code.hpp>

#include <cstddef>

namespace boost::redis::detail {

void process_ping_node(
   connection_logger& lgr,
   resp3::basic_node<std::string_view> const& nd,
   system::error_code& ec)
{
   switch (nd.data_type) {
      case resp3::type::simple_error: ec = redis::error::resp3_simple_error; break;
      case resp3::type::blob_error:   ec = redis::error::resp3_blob_error; break;
      default:                        ;
   }

   if (ec) {
      lgr.log(logger::level::info, "Health checker: server answered ping with an error", nd.value);
   }
}

inline any_adapter make_ping_adapter(connection_logger& lgr)
{
   return any_adapter{
      [&lgr](any_adapter::parse_event evt, resp3::node_view const& nd, system::error_code& ec) {
         if (evt == any_adapter::parse_event::node)
            process_ping_node(lgr, nd, ec);
      }};
}

writer_action writer_fsm::resume(
   connection_state& st,
   system::error_code ec,
   std::size_t bytes_written,
   asio::cancellation_type_t cancel_state)
{
   switch (resume_point_) {
      BOOST_REDIS_CORO_INITIAL

      for (;;) {
         // Attempt to write while we have requests ready to send
         while (st.mpx.prepare_write() != 0u) {
            // Write an entire message. We can't use asio::async_write because we want
            // to apply timeouts to individual write operations
            write_offset_ = 0u;
            while (write_offset_ < st.mpx.get_write_buffer().size()) {
               // Write what we can. If nothing has been written for the health check
               // interval, we consider the connection as failed
               BOOST_REDIS_YIELD(
                  resume_point_,
                  1,
                  writer_action::write_some(write_offset_, st.cfg.health_check_interval))

               // Commit the received bytes. Do it before error checking to account for partial success
               // TODO: I don't like how this is structured
               write_offset_ += bytes_written;
               if (write_offset_ >= st.mpx.get_write_buffer().size())
                  st.mpx.commit_write();

               // Check for cancellations
               if (is_terminal_cancel(cancel_state)) {
                  st.logger.trace("Writer task: cancelled (1).");
                  return system::error_code(asio::error::operation_aborted);
               }

               // Log what we wrote
               st.logger.on_write(ec, bytes_written);

               // Check for errors
               // TODO: translate operation_aborted to another error code for clarity.
               // pong_timeout is probably not the best option here - maybe write_timeout?
               if (ec) {
                  return ec;
               }
            }
         }

         // No more requests ready to be written. Wait for more, or until we need to send a PING
         BOOST_REDIS_YIELD(resume_point_, 2, writer_action::wait(st.cfg.health_check_interval))

         // Check for cancellations
         if (is_terminal_cancel(cancel_state)) {
            st.logger.trace("Writer task: cancelled (2).");
            return system::error_code(asio::error::operation_aborted);
         }

         // If we weren't notified, it's because there is no data and we should send a health check
         if (!ec) {
            auto elem = make_elem(st.ping_req, make_ping_adapter(st.logger));
            elem->set_done_callback([] { });
            st.mpx.add(elem);
         }
      }
   }

   // We should never reach here
   BOOST_ASSERT(false);
   return system::error_code();
}

}  // namespace boost::redis::detail

#endif
