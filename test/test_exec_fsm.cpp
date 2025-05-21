//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/detail/exec_fsm.hpp>
#include <boost/redis/detail/multiplexer.hpp>
#include <boost/redis/request.hpp>

#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/error.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/system/error_code.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <ostream>
#include <utility>

using namespace boost::redis;
namespace asio = boost::asio;
using detail::exec_fsm;
using detail::multiplexer;
using detail::exec_action_type;
using detail::exec_action;
using boost::system::error_code;
using boost::asio::cancellation_type_t;

// Operators
namespace boost::redis::detail {

std::ostream& operator<<(std::ostream& os, exec_action_type type)
{
   switch (type) {
      case exec_action_type::immediate:         return os << "exec_action_type::immediate";
      case exec_action_type::done:              return os << "exec_action_type::done";
      case exec_action_type::write:             return os << "exec_action_type::write";
      case exec_action_type::wait_for_response: return os << "exec_action_type::wait_for_response";
      case exec_action_type::cancel_run:        return os << "exec_action_type::cancel_run";
      default:                                  return os << "<unknown exec_action_type>";
   }
}

bool operator==(exec_action lhs, exec_action rhs) noexcept
{
   if (lhs.type() != rhs.type())
      return false;
   else if (lhs.type() == exec_action_type::done)
      return lhs.bytes_read() == rhs.bytes_read() && lhs.error() == rhs.error();
   else
      return true;
}

std::ostream& operator<<(std::ostream& os, exec_action act)
{
   os << "exec_action{ .type=" << act.type();
   if (act.type() == exec_action_type::done)
      os << ", .bytes_read=" << act.bytes_read() << ", .error=" << act.error();
   return os << " }";
}

}  // namespace boost::redis::detail

namespace {

// A helper to create a request and its associated elem
struct elem_and_request {
   request req;
   std::size_t done_calls{0u};  // number of times the done callback has been invoked
   std::shared_ptr<multiplexer::elem> elm;
   std::weak_ptr<multiplexer::elem> weak_elm;  // check that we free memory

   elem_and_request(request::config cfg = {})
   : req(cfg)
   {
      // Empty requests are not valid. The request needs to be populated before creating the element
      req.push("get", "mykey");

      elm = std::make_shared<multiplexer::elem>(
         req,
         [](std::size_t, resp3::node_view const&, error_code&) { });
      elm->set_done_callback([this] {
         ++done_calls;
      });

      weak_elm = elm;
   }
};

// The happy path
void test_success()
{
   // Setup
   multiplexer mpx;
   elem_and_request input;
   exec_fsm fsm(mpx, std::move(input.elm));
   error_code ec;

   // Initiate
   auto act = fsm.resume(true, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_action_type::write);

   // We should now wait for a response
   act = fsm.resume(true, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_action_type::wait_for_response);

   // Simulate a successful write
   BOOST_TEST_EQ(mpx.prepare_write(), 1u);  // one request was placed in the packet to write
   BOOST_TEST_EQ(mpx.commit_write(), 0u);   // all requests expect a response

   // Simulate a successful read
   mpx.get_read_buffer() = "$5\r\nhello\r\n";
   auto req_status = mpx.commit_read(ec);
   BOOST_TEST_EQ(ec, error_code());
   BOOST_TEST_EQ(req_status.first.value(), false);  // it wasn't a push
   BOOST_TEST_EQ(req_status.second, 11u);           // the entire buffer was consumed
   BOOST_TEST_EQ(input.done_calls, 1u);

   // This will awaken the exec operation, and should complete the operation
   act = fsm.resume(true, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_action(error_code(), 11u));

   // All memory should have been freed by now
   BOOST_TEST(input.weak_elm.expired());
}

// The request was configured to be cancelled on connection error, and the connection is closed
void test_cancel_if_not_connected()
{
   // Setup
   multiplexer mpx;
   request::config cfg;
   cfg.cancel_if_not_connected = true;
   elem_and_request input(cfg);
   exec_fsm fsm(mpx, std::move(input.elm));

   // Initiate. We're not connected, so the request gets cancelled
   auto act = fsm.resume(false, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_action_type::immediate);

   act = fsm.resume(false, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_action(error::not_connected));

   // We didn't leave memory behind
   BOOST_TEST(input.weak_elm.expired());
}

// The connection is closed when we start the request, but the request was configured to wait
void test_not_connected()
{
   // Setup
   multiplexer mpx;
   elem_and_request input;
   exec_fsm fsm(mpx, std::move(input.elm));
   error_code ec;

   // Initiate
   auto act = fsm.resume(false, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_action_type::write);

   // We should now wait for a response
   act = fsm.resume(true, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_action_type::wait_for_response);

   // Simulate a successful write
   BOOST_TEST_EQ(mpx.prepare_write(), 1u);  // one request was placed in the packet to write
   BOOST_TEST_EQ(mpx.commit_write(), 0u);   // all requests expect a response

   // Simulate a successful read
   mpx.get_read_buffer() = "$5\r\nhello\r\n";
   auto req_status = mpx.commit_read(ec);
   BOOST_TEST_EQ(ec, error_code());
   BOOST_TEST_EQ(req_status.first.value(), false);  // it wasn't a push
   BOOST_TEST_EQ(req_status.second, 11u);           // the entire buffer was consumed
   BOOST_TEST_EQ(input.done_calls, 1u);

   // This will awaken the exec operation, and should complete the operation
   act = fsm.resume(true, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_action(error_code(), 11u));

   // All memory should have been freed by now
   BOOST_TEST(input.weak_elm.expired());
}

//
// Cancellations
//

#define BOOST_TEST_EQ_MSG(lhs, rhs, msg)                                                     \
   if (!BOOST_TEST_EQ(lhs, rhs)) {                                                           \
      BOOST_LIGHTWEIGHT_TEST_OSTREAM << "Failure happened in context: " << msg << std::endl; \
   }

// If the request is waiting, all cancellation types are supported
void test_cancel_waiting()
{
   constexpr struct {
      const char* name;
      asio::cancellation_type_t type;
   } test_cases[] = {
      {"terminal", asio::cancellation_type_t::terminal                                     },
      {"partial",  asio::cancellation_type_t::partial                                      },
      {"total",    asio::cancellation_type_t::total                                        },
      {"mixed",    asio::cancellation_type_t::partial | asio::cancellation_type_t::terminal},
      {"all",      asio::cancellation_type_t::all                                          },
   };

   for (const auto& tc : test_cases) {
      // Setup
      multiplexer mpx;
      elem_and_request input, input2;
      exec_fsm fsm(mpx, std::move(input.elm));

      // Another request enters the multiplexer, so it's busy when we start
      mpx.add(input2.elm);
      BOOST_TEST_EQ_MSG(mpx.prepare_write(), 1u, tc.name);

      // Initiate and wait
      auto act = fsm.resume(true, cancellation_type_t::none);
      BOOST_TEST_EQ_MSG(act, exec_action_type::write, tc.name);

      act = fsm.resume(true, cancellation_type_t::none);
      BOOST_TEST_EQ_MSG(act, exec_action_type::wait_for_response, tc.name);

      // We get notified because the request got cancelled
      act = fsm.resume(true, tc.type);
      BOOST_TEST_EQ_MSG(act, exec_action(asio::error::operation_aborted), tc.name);
      BOOST_TEST_EQ_MSG(input.weak_elm.expired(), true, tc.name);  // we didn't leave memory behind
   }
}

// If the request is being processed and terminal cancellation got requested, we cancel the connection
void test_cancel_not_waiting_terminal()
{
   // Setup
   multiplexer mpx;
   elem_and_request input;
   exec_fsm fsm(mpx, std::move(input.elm));

   // Initiate
   auto act = fsm.resume(false, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_action_type::write);

   act = fsm.resume(true, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_action_type::wait_for_response);

   // The multiplexer starts writing the request
   BOOST_TEST_EQ(mpx.prepare_write(), 1u);  // one request was placed in the packet to write

   // A cancellation arrives
   act = fsm.resume(true, cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, exec_action_type::cancel_run);
   act = fsm.resume(true, cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, exec_action(asio::error::operation_aborted));

   // The object needs to survive here, otherwise an inconsistent connection state is created
}

// TODO: cancel other types not waiting

}  // namespace

int main()
{
   test_success();
   test_cancel_if_not_connected();
   test_not_connected();
   test_cancel_waiting();
   test_cancel_not_waiting_terminal();

   return boost::report_errors();
}
