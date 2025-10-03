//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/detail/reader_fsm.hpp>

#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/error.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/system/error_code.hpp>

#include <string_view>

namespace net = boost::asio;
namespace redis = boost::redis;
using boost::system::error_code;
using net::cancellation_type_t;
using redis::detail::reader_fsm;
using redis::detail::multiplexer;
using redis::generic_response;
using redis::any_adapter;
using redis::config;
using action = redis::detail::reader_fsm::action;

// Operators
namespace boost::redis::detail {

extern auto to_string(reader_fsm::action::type t) noexcept -> char const*;

std::ostream& operator<<(std::ostream& os, reader_fsm::action::type t)
{
   os << to_string(t);
   return os;
}

}  // namespace boost::redis::detail

namespace {

// Copy data into the multiplexer with the following steps
//
//   1. get_read_buffer
//   2. Copy data in the buffer from 2.
//
// This is used in the reader_fsm tests.
void copy_to(multiplexer& mpx, std::string_view data)
{
   auto const buffer = mpx.get_prepared_read_buffer();
   BOOST_ASSERT(buffer.size() >= data.size());
   std::copy(data.cbegin(), data.cend(), buffer.begin());
}

void test_push()
{
   multiplexer mpx;
   generic_response resp;
   mpx.set_receive_adapter(any_adapter{resp});
   reader_fsm fsm{mpx};
   error_code ec;
   action act;

   // Initiate
   act = fsm.resume(0, ec, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::read_some);

   // The fsm is asking for data.
   std::string const payload =
      ">1\r\n+msg1\r\n"
      ">1\r\n+msg2 \r\n"
      ">1\r\n+msg3  \r\n";

   copy_to(mpx, payload);

   // Deliver the 1st push
   act = fsm.resume(payload.size(), ec, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::notify_push_receiver);
   BOOST_TEST_EQ(act.push_size_, 11u);
   BOOST_TEST_EQ(act.ec_, error_code());

   // Deliver the 2st push
   act = fsm.resume(0, ec, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::notify_push_receiver);
   BOOST_TEST_EQ(act.push_size_, 12u);
   BOOST_TEST_EQ(act.ec_, error_code());

   // Deliver the 3rd push
   act = fsm.resume(0, ec, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::notify_push_receiver);
   BOOST_TEST_EQ(act.push_size_, 13u);
   BOOST_TEST_EQ(act.ec_, error_code());

   // All pushes were delivered so the fsm should demand more data
   act = fsm.resume(0, ec, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::read_some);
   BOOST_TEST_EQ(act.ec_, error_code());
}

void test_read_needs_more()
{
   multiplexer mpx;
   generic_response resp;
   mpx.set_receive_adapter(any_adapter{resp});
   reader_fsm fsm{mpx};
   error_code ec;
   action act;

   // Initiate
   act = fsm.resume(0, ec, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::read_some);

   // Split the incoming message in three random parts and deliver
   // them to the reader individually.
   std::string const msg[] = {">3\r", "\n+msg1\r\n+ms", "g2\r\n+msg3\r\n"};

   // Passes the first part to the fsm.
   copy_to(mpx, msg[0]);
   act = fsm.resume(msg[0].size(), ec, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::needs_more);
   BOOST_TEST_EQ(act.ec_, error_code());

   // Passes the second part to the fsm.
   copy_to(mpx, msg[1]);
   act = fsm.resume(msg[1].size(), ec, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::needs_more);
   BOOST_TEST_EQ(act.ec_, error_code());

   // Passes the third and last part to the fsm, next it should ask us
   // to deliver the message.
   copy_to(mpx, msg[2]);
   act = fsm.resume(msg[2].size(), ec, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::notify_push_receiver);
   BOOST_TEST_EQ(act.push_size_, msg[0].size() + msg[1].size() + msg[2].size());
   BOOST_TEST_EQ(act.ec_, error_code());

   // All pushes were delivered so the fsm should demand more data
   act = fsm.resume(0, ec, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::read_some);
   BOOST_TEST_EQ(act.ec_, error_code());
}

void test_read_error()
{
   multiplexer mpx;
   generic_response resp;
   mpx.set_receive_adapter(any_adapter{resp});
   reader_fsm fsm{mpx};
   error_code ec;
   action act;

   // Initiate
   act = fsm.resume(0, ec, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::read_some);

   // The fsm is asking for data.
   std::string const payload = ">1\r\n+msg1\r\n";
   copy_to(mpx, payload);

   // Deliver the data
   act = fsm.resume(payload.size(), {net::error::operation_aborted}, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::done);
   BOOST_TEST_EQ(act.ec_, error_code{net::error::operation_aborted});
}

void test_parse_error()
{
   multiplexer mpx;
   generic_response resp;
   mpx.set_receive_adapter(any_adapter{resp});
   reader_fsm fsm{mpx};
   error_code ec;
   action act;

   // Initiate
   act = fsm.resume(0, ec, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::read_some);

   // The fsm is asking for data.
   std::string const payload = ">a\r\n";
   copy_to(mpx, payload);

   // Deliver the data
   act = fsm.resume(payload.size(), {}, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::done);
   BOOST_TEST_EQ(act.ec_, error_code{redis::error::not_a_number});
}

void test_push_deliver_error()
{
   multiplexer mpx;
   generic_response resp;
   mpx.set_receive_adapter(any_adapter{resp});
   reader_fsm fsm{mpx};
   error_code ec;
   action act;

   // Initiate
   act = fsm.resume(0, ec, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::read_some);

   // The fsm is asking for data.
   std::string const payload = ">1\r\n+msg1\r\n";
   copy_to(mpx, payload);

   // Deliver the data
   act = fsm.resume(payload.size(), {}, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::notify_push_receiver);
   BOOST_TEST_EQ(act.ec_, error_code());

   // Resumes from notifying a push with an error.
   act = fsm.resume(0, net::error::operation_aborted, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::done);
   BOOST_TEST_EQ(act.ec_, error_code{net::error::operation_aborted});
}

void test_max_read_buffer_size()
{
   config cfg;
   cfg.read_buffer_append_size = 5;
   cfg.max_read_size = 7;

   multiplexer mpx;
   mpx.set_config(cfg);
   generic_response resp;
   mpx.set_receive_adapter(any_adapter{resp});
   reader_fsm fsm{mpx};
   error_code ec;
   action act;

   // Initiate
   act = fsm.resume(0, ec, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::read_some);

   // Passes the first part to the fsm.
   std::string const part1 = ">3\r\n";
   copy_to(mpx, part1);
   act = fsm.resume(part1.size(), {}, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::done);
   BOOST_TEST_EQ(act.ec_, redis::error::exceeds_maximum_read_buffer_size);
}

// Cancellations
void test_cancel_after_read()
{
   multiplexer mpx;
   generic_response resp;
   mpx.set_receive_adapter(any_adapter{resp});
   reader_fsm fsm{mpx};
   error_code ec;
   action act;

   // Initiate
   act = fsm.resume(0, ec, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::read_some);

   // Deliver a push, and notify a cancellation.
   // This can happen if the cancellation signal arrives before the read handler runs
   constexpr std::string_view payload = ">1\r\n+msg1\r\n";
   copy_to(mpx, payload);
   act = fsm.resume(payload.size(), ec, cancellation_type_t::terminal);
   BOOST_TEST_EQ(act.type_, action::type::done);
   BOOST_TEST_EQ(act.push_size_, 0u);
   BOOST_TEST_EQ(act.ec_, net::error::operation_aborted);
}

void test_cancel_after_push_delivery()
{
   multiplexer mpx;
   generic_response resp;
   mpx.set_receive_adapter(any_adapter{resp});
   reader_fsm fsm{mpx};
   error_code ec;
   action act;

   // Initiate
   act = fsm.resume(0, ec, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::read_some);

   // The fsm is asking for data.
   constexpr std::string_view payload =
      ">1\r\n+msg1\r\n"
      ">1\r\n+msg2 \r\n";

   copy_to(mpx, payload);

   // Deliver the 1st push
   act = fsm.resume(payload.size(), ec, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::notify_push_receiver);
   BOOST_TEST_EQ(act.push_size_, 11u);
   BOOST_TEST_EQ(act.ec_, error_code());

   // We got a cancellation after delivering it.
   // This can happen if the cancellation signal arrives before the channel send handler runs
   act = fsm.resume(0, ec, cancellation_type_t::terminal);
   BOOST_TEST_EQ(act.type_, action::type::done);
   BOOST_TEST_EQ(act.push_size_, 0u);
   BOOST_TEST_EQ(act.ec_, net::error::operation_aborted);
}

}  // namespace

int main()
{
   test_push();
   test_read_needs_more();

   test_read_error();
   test_parse_error();
   test_push_deliver_error();
   test_max_read_buffer_size();

   test_cancel_after_read();
   test_cancel_after_push_delivery();

   return boost::report_errors();
}
