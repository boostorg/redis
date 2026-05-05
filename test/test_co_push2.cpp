//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/co_connection.hpp>
#include <boost/redis/error.hpp>
#include <boost/redis/ignore.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/resp3/flat_tree.hpp>
#include <boost/redis/response.hpp>

#include <boost/capy/cond.hpp>
#include <boost/capy/ex/immediate.hpp>
#include <boost/capy/ex/this_coro.hpp>
#include <boost/capy/io_task.hpp>
#include <boost/capy/task.hpp>
#include <boost/capy/timeout.hpp>
#include <boost/capy/when_all.hpp>
#include <boost/capy/when_any.hpp>
#include <boost/core/lightweight_test.hpp>

#include "common.hpp"
#include "corosio_common.hpp"

#include <iterator>
#include <set>
#include <string>
#include <string_view>
#include <system_error>

namespace capy = boost::capy;
using namespace boost::redis;
using namespace boost::redis::test;
using namespace std::chrono_literals;
using error_code = std::error_code;
using resp3::flat_tree;
using resp3::node_view;
using resp3::type;

// Covers all receive functionality for the new co_connection API.

namespace {

// receive() is outstanding when a push is received
capy::task<> test_receive_waiting_for_push()
{
   resp3::flat_tree resp;
   co_connection conn{co_await capy::this_coro::executor};
   conn.set_receive_response(resp);

   auto exec1_fn = [&]() -> capy::io_task<> {
      request req;
      req.push("PING", "Message1");
      req.push("SUBSCRIBE", "test_receive_waiting_for_push");

      auto [ec] = co_await conn.exec(req, ignore);
      BOOST_TEST_EQ(ec, error_code());
      co_return {};
   };

   auto receive_then_exec2_fn = [&]() -> capy::io_task<> {
      auto [ec] = co_await conn.receive();
      BOOST_TEST_EQ(ec, error_code());
      BOOST_TEST_EQ(resp.get_total_msgs(), 1u);

      request req;
      req.push("PING", "Message2");

      auto [ec2] = co_await conn.exec(req, ignore);
      BOOST_TEST_EQ(ec2, error_code());
      co_return {};
   };

   auto work_fn = [&]() -> capy::io_task<> {
      auto [ec, a, b] = co_await capy::when_all(exec1_fn(), receive_then_exec2_fn());
      BOOST_TEST_EQ(ec, error_code());
      co_return {};
   };

   auto run_fn = [&]() -> capy::io_task<> {
      auto [ec] = co_await conn.run(make_test_config());
      BOOST_TEST_EQ(ec, capy_canceled_condition());
      co_return {};
   };

   auto result = co_await capy::when_any(work_fn(), run_fn());
   BOOST_TEST_EQ(result.index(), 1u);  // Work finished 1st
}

// A push is already available when receive() is called
capy::task<> test_receive_push_available()
{
   co_connection conn{co_await capy::this_coro::executor};
   resp3::flat_tree resp;
   conn.set_receive_response(resp);

   auto exec_fn = [&]() -> capy::io_task<> {
      // SUBSCRIBE doesn't have a response, but causes a push to be delivered.
      // Add a PING so the overall request has a response.
      // This ensures that when exec completes, the push has been delivered
      request req;
      req.push("SUBSCRIBE", "test_receive_push_available");
      req.push("PING", "message");

      auto [ec] = co_await conn.exec(req, ignore);
      BOOST_TEST_EQ(ec, error_code());

      auto [ec2] = co_await conn.receive();
      BOOST_TEST_EQ(ec2, error_code());
      BOOST_TEST_EQ(resp.get_total_msgs(), 1u);
      co_return {};
   };

   auto run_fn = [&]() -> capy::io_task<> {
      auto [ec] = co_await conn.run(make_test_config());
      BOOST_TEST_EQ(ec, capy_canceled_condition());
      co_return {};
   };

   auto result = co_await capy::when_any(exec_fn(), run_fn());
   BOOST_TEST_EQ(result.index(), 1u);  // Exec finished 1st
}

// receive() blocks only once if several messages are received in a batch
capy::task<> test_receive_batch()
{
   co_connection conn{co_await capy::this_coro::executor};
   resp3::flat_tree resp;
   conn.set_receive_response(resp);

   auto exec_fn = [&]() -> capy::io_task<> {
      // 1. Trigger pushes
      // This causes two messages to be delivered. The PING ensures that
      // the pushes have been read when exec completes
      request req;
      req.push("SUBSCRIBE", "test_receive_batch");
      req.push("SUBSCRIBE", "test_receive_batch");
      req.push("PING", "message");
      auto [ec] = co_await conn.exec(req, ignore);
      BOOST_TEST_EQ(ec, error_code());

      // 2. Receive both of them
      auto [ec2] = co_await conn.receive();
      BOOST_TEST_EQ(ec2, error_code());
      BOOST_TEST_EQ(resp.get_total_msgs(), 2u);

      // 3. Check that receive has consumed them by calling it again with a deadline
      auto [ec3] = co_await capy::timeout(conn.receive(), 50ms);
      BOOST_TEST_EQ(ec3, condition_wrapper{capy::cond::timeout});
      co_return {};
   };

   auto run_fn = [&]() -> capy::io_task<> {
      auto [ec] = co_await conn.run(make_test_config());
      BOOST_TEST_EQ(ec, capy_canceled_condition());
      co_return {};
   };

   auto result = co_await capy::when_any(exec_fn(), run_fn());
   BOOST_TEST_EQ(result.index(), 1u);  // Exec finished 1st
}

// receive() can be called several times in a row
capy::task<> test_receive_subsequent_calls()
{
   co_connection conn{co_await capy::this_coro::executor};
   resp3::flat_tree resp;
   conn.set_receive_response(resp);

   auto exec_fn = [&]() -> capy::io_task<> {
      // Send a SUBSCRIBE, which will trigger a push
      request req;
      req.push("SUBSCRIBE", "test_receive_subsequent_calls");
      auto [ec] = co_await conn.exec(req, ignore);
      BOOST_TEST_EQ(ec, error_code());

      // Receive the push
      auto [ec2] = co_await conn.receive();
      BOOST_TEST_EQ(ec2, error_code());
      BOOST_TEST_EQ(resp.get_total_msgs(), 1u);
      resp.clear();

      // Send another SUBSCRIBE, which will trigger another push
      auto [ec3] = co_await conn.exec(req, ignore);
      BOOST_TEST_EQ(ec3, error_code());

      // Receive the push
      auto [ec4] = co_await conn.receive();
      BOOST_TEST_EQ(ec4, error_code());
      BOOST_TEST_EQ(resp.get_total_msgs(), 1u);
      co_return {};
   };

   auto run_fn = [&]() -> capy::io_task<> {
      auto [ec] = co_await conn.run(make_test_config());
      BOOST_TEST_EQ(ec, capy_canceled_condition());
      co_return {};
   };

   auto result = co_await capy::when_any(exec_fn(), run_fn());
   BOOST_TEST_EQ(result.index(), 1u);  // Exec finished 1st
}

// receive() can be cancelled via stop token
capy::task<> test_receive_cancellation()
{
   co_connection conn{co_await capy::this_coro::executor};

   auto receive_fn = [&]() -> capy::io_task<> {
      auto [ec] = co_await conn.receive();
      BOOST_TEST_EQ(ec, capy_canceled_condition());
      co_return {};
   };

   auto result = co_await capy::when_any(receive_fn(), capy::ready());
   BOOST_TEST_EQ(result.index(), 2u);  // trigger finished 1st
}

// Reconnection doesn't cancel receive()
capy::task<> test_receive_reconnection()
{
   co_connection conn{co_await capy::this_coro::executor};
   resp3::flat_tree resp;
   conn.set_receive_response(resp);

   bool receive_finished = false;

   auto receive_fn = [&]() -> capy::io_task<> {
      auto [ec] = co_await conn.receive();
      BOOST_TEST_EQ(ec, error_code());
      receive_finished = true;
      co_return {};
   };

   // Trigger a reconnection, then trigger a push to make receive complete
   auto trigger_fn = [&]() -> capy::io_task<> {
      // Causes a reconnection
      request req_quit;
      req_quit.push("QUIT");
      auto [ec_quit] = co_await conn.exec(req_quit, ignore);
      static_cast<void>(ec_quit);  // QUIT may complete with success or an error; we don't care

      // Reconnection has happened by the time PING completes
      request req_ping;
      req_ping.get_config().cancel_if_unresponded = false;
      req_ping.push("PING", "test_receive_reconnection");
      auto [ec_ping] = co_await conn.exec(req_ping, ignore);
      BOOST_TEST_EQ(ec_ping, error_code());
      BOOST_TEST_NOT(receive_finished);

      // Generates a push
      request req_subscribe;
      req_subscribe.push("SUBSCRIBE", "test_receive_reconnection");
      auto [ec_sub] = co_await conn.exec(req_subscribe, ignore);
      BOOST_TEST_EQ(ec_sub, error_code());
      co_return {};
   };

   auto work_fn = [&]() -> capy::io_task<> {
      auto [ec, a, b] = co_await capy::when_all(receive_fn(), trigger_fn());
      BOOST_TEST_EQ(ec, error_code());
      co_return {};
   };

   auto run_fn = [&]() -> capy::io_task<> {
      auto [ec] = co_await conn.run(make_test_config());
      BOOST_TEST_EQ(ec, capy_canceled_condition());
      co_return {};
   };

   auto result = co_await capy::when_any(work_fn(), run_fn());
   BOOST_TEST_EQ(result.index(), 1u);  // Work finished 1st
}

// A push may be interleaved between regular responses.
// It is handed to the receive adapter (filtered out).
capy::task<> test_exec_push_interleaved()
{
   co_connection conn{co_await capy::this_coro::executor};
   resp3::flat_tree receive_resp;
   conn.set_receive_response(receive_resp);

   auto exec_fn = [&]() -> capy::io_task<> {
      request req;
      req.push("PING", "msg1");
      req.push("SUBSCRIBE", "test_exec_push_interleaved");
      req.push("PING", "msg2");

      response<std::string, std::string> resp;

      auto [ec] = co_await conn.exec(req, resp);
      BOOST_TEST_EQ(ec, error_code());
      BOOST_TEST_EQ(std::get<0>(resp).value(), "msg1");
      BOOST_TEST_EQ(std::get<1>(resp).value(), "msg2");
      co_return {};
   };

   auto receive_fn = [&]() -> capy::io_task<> {
      auto [ec] = co_await conn.receive();
      BOOST_TEST_EQ(ec, error_code());
      BOOST_TEST_EQ(receive_resp.get_total_msgs(), 1u);
      co_return {};
   };

   auto work_fn = [&]() -> capy::io_task<> {
      auto [ec, a, b] = co_await capy::when_all(exec_fn(), receive_fn());
      BOOST_TEST_EQ(ec, error_code());
      co_return {};
   };

   auto run_fn = [&]() -> capy::io_task<> {
      auto [ec] = co_await conn.run(make_test_config());
      BOOST_TEST_EQ(ec, capy_canceled_condition());
      co_return {};
   };

   auto result = co_await capy::when_any(work_fn(), run_fn());
   BOOST_TEST_EQ(result.index(), 1u);  // Work finished 1st
}

// An adapter that always errors
struct response_error_tag { };
response_error_tag error_tag_obj;

struct response_error_adapter {
   void on_init() { }
   void on_done() { }
   void on_node(node_view const&, error_code& ec) { ec = error::incompatible_size; }
};

auto boost_redis_adapt(response_error_tag&) { return response_error_adapter{}; }

// If the push adapter returns an error, the connection is torn down
capy::task<> test_push_adapter_error()
{
   co_connection conn{co_await capy::this_coro::executor};
   conn.set_receive_response(error_tag_obj);

   auto receive_fn = [&]() -> capy::io_task<> {
      // Will be cancelled by when_any
      auto [ec] = co_await conn.receive();
      BOOST_TEST_EQ(ec, capy_canceled_condition());
      co_return {};
   };

   // The request is cancelled because the PING response isn't processed
   // by the time the error is generated
   auto exec_fn = [&]() -> capy::io_task<> {
      request req;
      req.push("PING");
      req.push("SUBSCRIBE", "channel");
      req.push("PING");

      auto [ec] = co_await conn.exec(req, ignore);
      BOOST_TEST_EQ(ec, capy_canceled_condition());
      co_return {};
   };

   auto run_fn = [&]() -> capy::io_task<> {
      auto cfg = make_test_config();
      cfg.reconnect_wait_interval = 0s;  // so we can validate the generated error
      auto [ec] = co_await conn.run(cfg);
      BOOST_TEST_EQ(ec, error::incompatible_size);
      co_return {};
   };

   co_await capy::when_any(receive_fn(), exec_fn(), run_fn());
}

// A push response error triggers a reconnection
capy::task<> test_push_adapter_error_reconnection()
{
   co_connection conn{co_await capy::this_coro::executor};
   conn.set_receive_response(error_tag_obj);

   // receive() will be cancelled by when_any
   auto receive_fn = [&]() -> capy::io_task<> {
      auto [ec] = co_await conn.receive();
      BOOST_TEST_EQ(ec, capy_canceled_condition());
      co_return {};
   };

   auto exec_fn = [&]() -> capy::io_task<> {
      // The request is cancelled because the PING response isn't processed
      // by the time the error is generated
      request req;
      req.push("PING");
      req.push("SUBSCRIBE", "channel");
      req.push("PING");

      auto [ec1] = co_await conn.exec(req, ignore);
      BOOST_TEST_EQ(ec1, capy_canceled_condition());

      // This one will succeed after reconnection
      request req2;
      req2.push("PING", "msg2");
      req2.get_config().cancel_if_unresponded = false;

      response<std::string> resp;

      auto [ec2] = co_await conn.exec(req2, resp);
      BOOST_TEST_EQ(ec2, error_code());
      BOOST_TEST_EQ(std::get<0>(resp).value(), "msg2");
      co_return {};
   };

   auto run_fn = [&]() -> capy::io_task<> {
      auto [ec] = co_await conn.run(make_test_config());
      BOOST_TEST_EQ(ec, capy_canceled_condition());
      co_return {};
   };

   auto result = co_await capy::when_any(receive_fn(), exec_fn(), run_fn());
   BOOST_TEST_EQ(result.index(), 2u);  // exec finished after the receive cancel
}

// Tests the usual push consumer pattern that we recommend in the examples
capy::task<> test_push_consumer()
{
   co_connection conn{co_await capy::this_coro::executor};
   resp3::flat_tree resp;
   conn.set_receive_response(resp);

   auto consumer_fn = [&]() -> capy::io_task<> {
      while (true) {
         auto [ec] = co_await conn.receive();
         resp.clear();
         if (ec) {
            BOOST_TEST_EQ(ec, capy_canceled_condition());
            co_return {};
         }
      }
   };

   auto exec_fn = [&]() -> capy::io_task<> {
      request req1;
      req1.get_config().cancel_on_connection_lost = false;
      req1.push("PING", "Message1");

      request req2;
      req2.get_config().cancel_on_connection_lost = false;
      req2.push("SUBSCRIBE", "channel");

      const request* sequence[] =
         {&req1, &req2, &req2, &req1, &req2, &req1, &req2, &req2, &req1, &req2};
      for (const auto* r : sequence) {
         auto [ec] = co_await conn.exec(*r, ignore);
         BOOST_TEST_EQ(ec, error_code());
      }
      co_return {};
   };

   auto run_fn = [&]() -> capy::io_task<> {
      auto [ec] = co_await conn.run(make_test_config());
      BOOST_TEST_EQ(ec, capy_canceled_condition());
      co_return {};
   };

   auto result = co_await capy::when_any(consumer_fn(), exec_fn(), run_fn());
   BOOST_TEST_EQ(result.index(), 2u);  // exec finished 1st
}

// UNSUBSCRIBE and PUNSUBSCRIBE work
capy::task<> test_unsubscribe()
{
   co_connection conn{co_await capy::this_coro::executor};

   auto exec_fn = [&]() -> capy::io_task<> {
      response<std::string> resp_subscribe, resp_unsubscribe, resp_ping;

      // Subscribe to 3 channels and 2 patterns. Use CLIENT INFO to verify this took effect
      request req_subscribe;
      req_subscribe.push("SUBSCRIBE", "ch1", "ch2", "ch3");
      req_subscribe.push("PSUBSCRIBE", "ch1*", "ch2*");
      req_subscribe.push("CLIENT", "INFO");

      auto [ec_sub] = co_await conn.exec(req_subscribe, resp_subscribe);
      BOOST_TEST_EQ(ec_sub, error_code());
      BOOST_TEST(std::get<0>(resp_subscribe).has_value());
      BOOST_TEST_EQ(find_client_info(std::get<0>(resp_subscribe).value(), "sub"), "3");
      BOOST_TEST_EQ(find_client_info(std::get<0>(resp_subscribe).value(), "psub"), "2");

      // Then, unsubscribe from some of them, and verify again
      request req_unsubscribe;
      req_unsubscribe.push("UNSUBSCRIBE", "ch1");
      req_unsubscribe.push("PUNSUBSCRIBE", "ch2*");
      req_unsubscribe.push("CLIENT", "INFO");

      auto [ec_unsub] = co_await conn.exec(req_unsubscribe, resp_unsubscribe);
      BOOST_TEST_EQ(ec_unsub, error_code());
      BOOST_TEST(std::get<0>(resp_unsubscribe).has_value());
      BOOST_TEST_EQ(find_client_info(std::get<0>(resp_unsubscribe).value(), "sub"), "2");
      BOOST_TEST_EQ(find_client_info(std::get<0>(resp_unsubscribe).value(), "psub"), "1");

      // Finally, ping to verify that the connection is still usable
      request req_ping;
      req_ping.push("PING", "test_unsubscribe");

      auto [ec_ping] = co_await conn.exec(req_ping, resp_ping);
      BOOST_TEST_EQ(ec_ping, error_code());
      BOOST_TEST(std::get<0>(resp_ping).has_value());
      BOOST_TEST_EQ(std::get<0>(resp_ping).value(), "test_unsubscribe");
      co_return {};
   };

   auto run_fn = [&]() -> capy::io_task<> {
      auto [ec] = co_await conn.run(make_test_config());
      BOOST_TEST_EQ(ec, capy_canceled_condition());
      co_return {};
   };

   auto result = co_await capy::when_any(exec_fn(), run_fn());
   BOOST_TEST_EQ(result.index(), 1u);  // Exec finished 1st
}

void check_subscriptions(flat_tree const& resp_push)
{
   // Checks for the expected subscriptions and patterns after restoration
   std::set<std::string_view> seen_channels, seen_patterns;
   for (auto it = resp_push.begin(); it != resp_push.end();) {
      // The root element should be a push
      BOOST_TEST_EQ(it->data_type, type::push);
      BOOST_TEST_GE(it->aggregate_size, 2u);
      BOOST_TEST(++it != resp_push.end());

      // The next element should be the message type
      std::string_view msg_type = it->value;
      BOOST_TEST(++it != resp_push.end());

      // The next element is the channel or pattern
      if (msg_type == "subscribe")
         seen_channels.insert(it->value);
      else if (msg_type == "psubscribe")
         seen_patterns.insert(it->value);

      // Skip the rest of the nodes
      while (it != resp_push.end() && it->depth != 0u)
         ++it;
   }

   const std::string_view expected_channels[] = {"ch1", "ch3", "ch5"};
   const std::string_view expected_patterns[] = {"ch1*", "ch3*", "ch4*", "ch8*"};

   BOOST_TEST_ALL_EQ(
      seen_channels.begin(),
      seen_channels.end(),
      std::begin(expected_channels),
      std::end(expected_channels));
   BOOST_TEST_ALL_EQ(
      seen_patterns.begin(),
      seen_patterns.end(),
      std::begin(expected_patterns),
      std::end(expected_patterns));
}

capy::task<> test_pubsub_state_restoration()
{
   co_connection conn{co_await capy::this_coro::executor};
   flat_tree resp_push;
   conn.set_receive_response(resp_push);

   auto exec_fn = [&]() -> capy::io_task<> {
      // Subscribe to some channels and patterns
      request req1;
      req1.subscribe({"ch1", "ch2", "ch3"});              // active: 1, 2, 3
      req1.psubscribe({"ch1*", "ch2*", "ch3*", "ch4*"});  // active: 1, 2, 3, 4
      auto [ec1] = co_await conn.exec(req1, ignore);
      BOOST_TEST_EQ(ec1, error_code());

      // Unsubscribe from some channels and patterns.
      // Unsubscribing from a channel/pattern that we weren't subscribed to is OK.
      request req2;
      req2.unsubscribe({"ch2", "ch1", "ch5"});      // active: 3
      req2.punsubscribe({"ch2*", "ch4*", "ch9*"});  // active: 1, 3
      auto [ec2] = co_await conn.exec(req2, ignore);
      BOOST_TEST_EQ(ec2, error_code());

      // Subscribe to other channels/patterns.
      // Re-subscribing to channels/patterns we unsubscribed from is OK.
      // Subscribing to the same channel/pattern twice is OK.
      request req3;
      req3.subscribe({"ch1", "ch3", "ch5"});      // active: 1, 3, 5
      req3.psubscribe({"ch3*", "ch4*", "ch8*"});  // active: 1, 3, 4, 8

      // Subscriptions created by push() don't survive reconnection
      req3.push("SUBSCRIBE", "ch10");    // active: 1, 3, 5, 10
      req3.push("PSUBSCRIBE", "ch10*");  // active: 1, 3, 4, 8, 10

      // Validate that we're subscribed to what we expect
      req3.push("CLIENT", "INFO");

      response<std::string> resp3;

      auto [ec3] = co_await conn.exec(req3, resp3);
      BOOST_TEST_EQ(ec3, error_code());

      // We are subscribed to 4 channels and 5 patterns
      BOOST_TEST(std::get<0>(resp3).has_value());
      BOOST_TEST_EQ(find_client_info(std::get<0>(resp3).value(), "sub"), "4");
      BOOST_TEST_EQ(find_client_info(std::get<0>(resp3).value(), "psub"), "5");

      resp_push.clear();

      // Trigger a reconnection
      request req4;
      req4.push("QUIT");
      auto result = co_await conn.exec(req4, ignore);
      static_cast<void>(result);
      // we don't know if this request will complete successfully or not

      // Verify state after reconnection
      request req5;
      req5.push("CLIENT", "INFO");
      req5.get_config().cancel_if_unresponded = false;

      response<std::string> resp5;

      auto [ec5] = co_await conn.exec(req5, resp5);
      BOOST_TEST_EQ(ec5, error_code());

      // We are subscribed to 3 channels and 4 patterns (1 of each didn't survive reconnection)
      BOOST_TEST(std::get<0>(resp5).has_value());
      BOOST_TEST_EQ(find_client_info(std::get<0>(resp5).value(), "sub"), "3");
      BOOST_TEST_EQ(find_client_info(std::get<0>(resp5).value(), "psub"), "4");

      // We have received pushes confirming it
      check_subscriptions(resp_push);

      co_return {};
   };

   auto run_fn = [&]() -> capy::io_task<> {
      auto [ec] = co_await conn.run(make_test_config());
      BOOST_TEST_EQ(ec, capy_canceled_condition());
      co_return {};
   };

   auto result = co_await capy::when_any(exec_fn(), run_fn());
   BOOST_TEST_EQ(result.index(), 1u);  // Exec finished 1st
}

}  // namespace

int main()
{
   run_coroutine_test(test_receive_waiting_for_push());
   run_coroutine_test(test_receive_push_available());
   run_coroutine_test(test_receive_batch());
   run_coroutine_test(test_receive_subsequent_calls());
   run_coroutine_test(test_receive_cancellation());
   run_coroutine_test(test_receive_reconnection());
   run_coroutine_test(test_exec_push_interleaved());
   run_coroutine_test(test_push_adapter_error());
   run_coroutine_test(test_push_adapter_error_reconnection());
   run_coroutine_test(test_push_consumer());
   run_coroutine_test(test_unsubscribe());
   run_coroutine_test(test_pubsub_state_restoration());

   return boost::report_errors();
}
