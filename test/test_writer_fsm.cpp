//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/detail/connection_logger.hpp>
#include <boost/redis/detail/connection_state.hpp>
#include <boost/redis/detail/multiplexer.hpp>
#include <boost/redis/detail/writer_fsm.hpp>
#include <boost/redis/logger.hpp>
#include <boost/redis/request.hpp>

#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/error.hpp>
#include <boost/assert.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/system/error_code.hpp>

#include "sansio_utils.hpp"

#include <chrono>
#include <memory>
#include <ostream>

using namespace boost::redis;
namespace asio = boost::asio;
using detail::writer_fsm;
using detail::multiplexer;
using detail::writer_action_type;
using detail::consume_result;
using detail::writer_action;
using detail::connection_state;
using boost::system::error_code;
using boost::asio::cancellation_type_t;
using detail::connection_logger;
using namespace std::chrono_literals;

// Operators
static const char* to_string(writer_action_type value)
{
   switch (value) {
      case writer_action_type::done:       return "writer_action_type::done";
      case writer_action_type::write_some: return "writer_action_type::write";
      case writer_action_type::wait:       return "writer_action_type::wait";
      default:                             return "<unknown writer_action_type>";
   }
}

namespace boost::redis::detail {

std::ostream& operator<<(std::ostream& os, writer_action_type type)
{
   os << to_string(type);
   return os;
}

bool operator==(const writer_action& lhs, const writer_action& rhs) noexcept
{
   if (lhs.type() != rhs.type())
      return false;
   switch (lhs.type()) {
      case writer_action_type::done: return lhs.error() == rhs.error();
      case writer_action_type::write_some:
         return lhs.write_offset() == rhs.write_offset() && lhs.timeout() == rhs.timeout();
      case writer_action_type::wait: return lhs.timeout() == rhs.timeout();
      default:                       BOOST_ASSERT(false);
   }
   return false;
}

std::ostream& operator<<(std::ostream& os, const writer_action& act)
{
   auto t = act.type();
   os << "writer_action{ .type=" << t;
   switch (t) {
      case writer_action_type::done: os << ", .error=" << act.error(); break;
      case writer_action_type::write_some:
         os << ", .write_buffer=" << act.write_offset()
            << ", timeout=" << to_milliseconds(act.timeout()) << "ms";
         break;
      case writer_action_type::wait:
         os << ", .timeout=" << to_milliseconds(act.timeout()) << "ms";
         break;
      default: BOOST_ASSERT(false);
   }

   return os << " }";
}

}  // namespace boost::redis::detail

namespace {

// A helper to create a request and its associated elem
struct test_elem {
   request req;
   bool done{false};
   std::shared_ptr<multiplexer::elem> elm;

   test_elem()
   {
      // Empty requests are not valid. The request needs to be populated before creating the element
      req.push("get", "mykey");
      elm = std::make_shared<multiplexer::elem>(req, any_adapter{});

      elm->set_done_callback([this] {
         done = true;
      });
   }
};

struct fixture : detail::log_fixture {
   connection_state st{make_logger()};
   writer_fsm fsm;

   fixture()
   {
      st.cfg.health_check_id = "my_health_check";
      st.cfg.health_check_interval = 4s;
   }
};

// A single request is written, then we wait and repeat
void test_single_request()
{
   // Setup
   fixture fix;
   test_elem item1, item2;

   // A request arrives before the writer starts
   fix.st.mpx.add(item1.elm);

   // Start. A write is triggered, and the request is marked as staged
   auto act = fix.fsm.resume(fix.st, error_code(), 0u, cancellation_type_t::none);
   BOOST_TEST_EQ(act, writer_action::write_some(0u, 4s));
   BOOST_TEST(item1.elm->is_staged());

   // The write completes successfully. The request is written, and we go back to sleep.
   act = fix.fsm
            .resume(fix.st, error_code(), item1.req.payload().size(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, writer_action::wait(4s));
   BOOST_TEST(item1.elm->is_written());

   // Another request arrives
   fix.st.mpx.add(item2.elm);

   // The wait is cancelled to signal we've got a new request
   act = fix.fsm.resume(fix.st, asio::error::operation_aborted, 0u, cancellation_type_t::none);
   BOOST_TEST_EQ(act, writer_action::write_some(0u, 4s));
   BOOST_TEST(item2.elm->is_staged());

   // Write successful
   act = fix.fsm
            .resume(fix.st, error_code(), item2.req.payload().size(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, writer_action::wait(4s));
   BOOST_TEST(item2.elm->is_written());

   // Logs
   fix.check_log({
      {logger::level::info, "Writer task: 24 bytes written."},
      {logger::level::info, "Writer task: 24 bytes written."},
   });
}

// If a request arrives while we're performing a write, we don't get back to sleep
void test_request_arrives_while_writing()
{
   // Setup
   fixture fix;
   test_elem item1, item2;

   // A request arrives before the writer starts
   fix.st.mpx.add(item1.elm);

   // Start. A write is triggered, and the request is marked as staged
   auto act = fix.fsm.resume(fix.st, error_code(), 0u, cancellation_type_t::none);
   BOOST_TEST_EQ(act, writer_action::write_some(0u, 4s));
   BOOST_TEST(item1.elm->is_staged());

   // While the write is outstanding, a new request arrives
   fix.st.mpx.add(item2.elm);

   // The write completes successfully. The request is written,
   // and we start writing the new one
   act = fix.fsm
            .resume(fix.st, error_code(), item1.req.payload().size(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, writer_action::write_some(0u, 4s));
   BOOST_TEST(item1.elm->is_written());
   BOOST_TEST(item2.elm->is_staged());

   // Write successful
   act = fix.fsm
            .resume(fix.st, error_code(), item2.req.payload().size(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, writer_action::wait(4s));
   BOOST_TEST(item2.elm->is_written());

   // Logs
   fix.check_log({
      {logger::level::info, "Writer task: 24 bytes written."},
      {logger::level::info, "Writer task: 24 bytes written."},
   });
}

// If there is no request when the writer starts, we wait for it
void test_no_request_at_startup()
{
   // Setup
   fixture fix;
   test_elem item;

   // Start. There is no request, so we wait
   auto act = fix.fsm.resume(fix.st, error_code(), 0u, cancellation_type_t::none);
   BOOST_TEST_EQ(act, writer_action::wait(4s));

   // A request arrives
   fix.st.mpx.add(item.elm);

   // The wait is cancelled to signal we've got a new request
   act = fix.fsm.resume(fix.st, asio::error::operation_aborted, 0u, cancellation_type_t::none);
   BOOST_TEST_EQ(act, writer_action::write_some(0u, 4s));
   BOOST_TEST(item.elm->is_staged());

   // Write successful
   act = fix.fsm.resume(fix.st, error_code(), item.req.payload().size(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, writer_action::wait(4s));
   BOOST_TEST(item.elm->is_written());

   // Logs
   fix.check_log({
      {logger::level::info, "Writer task: 24 bytes written."},
   });
}

// A write error makes the writer exit
void test_write_error()
{
   // Setup
   fixture fix;
   test_elem item;

   // A request arrives before the writer starts
   fix.st.mpx.add(item.elm);

   // Start. A write is triggered, and the request is marked as staged
   auto act = fix.fsm.resume(fix.st, error_code(), 0u, cancellation_type_t::none);
   BOOST_TEST_EQ(act, writer_action::write_some(0u, 4s));
   BOOST_TEST(item.elm->is_staged());

   // The write completes with an error (possibly with partial success).
   // The request is still staged, and the writer exits.
   // Use an error we control so we can check logs
   act = fix.fsm.resume(fix.st, error::empty_field, 2u, cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code(error::empty_field));
   BOOST_TEST(item.elm->is_staged());

   // Logs
   fix.check_log({
      {logger::level::info, "Writer task error: Expected field value is empty. [boost.redis:5]"},
   });
}

// A write is cancelled
void test_cancel_write()
{
   // Setup
   fixture fix;
   test_elem item;

   // A request arrives before the writer starts
   fix.st.mpx.add(item.elm);

   // Start. A write is triggered
   auto act = fix.fsm.resume(fix.st, error_code(), 0u, cancellation_type_t::none);
   BOOST_TEST_EQ(act, writer_action::write_some(0u, 4s));
   BOOST_TEST(item.elm->is_staged());

   // Write cancelled and failed with operation_aborted
   act = fix.fsm.resume(fix.st, asio::error::operation_aborted, 2u, cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, error_code(asio::error::operation_aborted));
   BOOST_TEST(item.elm->is_staged());

   // Logs
   fix.check_log({
      {logger::level::debug, "Writer task: cancelled (1)."},
   });
}

// A write is cancelled after completing but before the handler is dispatched
void test_cancel_write_edge()
{
   // Setup
   fixture fix;
   test_elem item;

   // A request arrives before the writer starts
   fix.st.mpx.add(item.elm);

   // Start. A write is triggered
   auto act = fix.fsm.resume(fix.st, error_code(), 0u, cancellation_type_t::none);
   BOOST_TEST_EQ(act, writer_action::write_some(0u, 4s));
   BOOST_TEST(item.elm->is_staged());

   // Write cancelled but without error
   act = fix.fsm
            .resume(fix.st, error_code(), item.req.payload().size(), cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, error_code(asio::error::operation_aborted));
   BOOST_TEST(item.elm->is_written());

   // Logs
   fix.check_log({
      {logger::level::debug, "Writer task: cancelled (1)."},
   });
}

// The wait was cancelled because of per-operation cancellation (rather than a notification)
void test_cancel_wait()
{
   // Setup
   fixture fix;
   test_elem item;

   // Start. There is no request, so we wait
   auto act = fix.fsm.resume(fix.st, error_code(), 0u, cancellation_type_t::none);
   BOOST_TEST_EQ(act, writer_action::wait(4s));

   // Sanity check: the writer doesn't touch the multiplexer after a cancellation
   fix.st.mpx.add(item.elm);

   // Cancel the wait, setting the cancellation state
   act = fix.fsm.resume(
      fix.st,
      asio::error::operation_aborted,
      0u,
      asio::cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, error_code(asio::error::operation_aborted));
   BOOST_TEST(item.elm->is_waiting());

   // Logs
   fix.check_log({
      {logger::level::debug, "Writer task: cancelled (2)."},
   });
}

}  // namespace

int main()
{
   test_single_request();
   test_request_arrives_while_writing();
   test_no_request_at_startup();

   test_write_error();

   test_cancel_write();
   test_cancel_write_edge();
   test_cancel_wait();

   return boost::report_errors();
}
