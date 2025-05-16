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
#include <boost/core/lightweight_test.hpp>
#include <boost/system/error_code.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <ostream>
#include <utility>

using namespace boost::redis;
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

void test_success()
{
   // Setup
   multiplexer mpx;
   request req;
   req.push("get", "mykey");
   int done_calls = 0, adapter_calls = 0;
   auto elm = std::make_shared<multiplexer::elem>(
      req,
      [&adapter_calls](std::size_t, resp3::node_view const&, error_code&) {
         ++adapter_calls;
      });
   elm->set_done_callback([&done_calls] {
      ++done_calls;
   });
   std::weak_ptr<multiplexer::elem> weak_elm(elm);
   exec_fsm fsm(mpx, std::move(elm));
   error_code ec;

   // Initiate
   auto act = fsm.resume(true, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_action_type::write);

   // After being notified, we should wait for a response
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
   BOOST_TEST_EQ(done_calls, 1);

   // This will awaken the exec operation, and should complete the operation
   act = fsm.resume(true, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_action(error_code(), 11u));

   // All memory should have been freed by now
   BOOST_TEST(weak_elm.expired());
}

}  // namespace

int main()
{
   test_success();
   return boost::report_errors();
}
