/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>
#include <boost/redis/logger.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/resp3/flat_tree.hpp>
#include <boost/redis/response.hpp>

#include <boost/asio/error.hpp>
#include <boost/asio/experimental/channel_error.hpp>
#include <boost/core/lightweight_test.hpp>

#include "common.hpp"

#include <cstddef>
#include <iostream>
#include <iterator>
#include <set>
#include <string>
#include <string_view>

namespace net = boost::asio;
using namespace boost::redis;
using namespace std::chrono_literals;
using boost::system::error_code;
using resp3::flat_tree;
using resp3::node_view;
using resp3::type;

// Covers all receive functionality except for the deprecated
// async_receive and receive functions.

namespace {

void test_receives_push_waiting_resps()
{
   request req1;
   req1.push("HELLO", 3);
   req1.push("PING", "Message1");

   request req2;
   req2.push("SUBSCRIBE", "channel");

   request req3;
   req3.push("PING", "Message2");
   req3.push("QUIT");

   net::io_context ioc;

   auto conn = std::make_shared<connection>(ioc);

   bool push_received = false, c1_called = false, c2_called = false, c3_called = false;

   auto c3 = [&](error_code ec, std::size_t) {
      c3_called = true;
      std::cout << "c3: " << ec.message() << std::endl;
   };

   auto c2 = [&, conn](error_code ec, std::size_t) {
      c2_called = true;
      BOOST_TEST_EQ(ec, error_code());
      conn->async_exec(req3, ignore, c3);
   };

   auto c1 = [&, conn](error_code ec, std::size_t) {
      c1_called = true;
      BOOST_TEST_EQ(ec, error_code());
      conn->async_exec(req2, ignore, c2);
   };

   conn->async_exec(req1, ignore, c1);

   run(conn, make_test_config(), {});

   conn->async_receive2([&, conn](error_code ec) {
      std::cout << "async_receive2" << std::endl;
      BOOST_TEST_EQ(ec, error_code());
      push_received = true;
      conn->cancel();
   });

   ioc.run_for(test_timeout);

   BOOST_TEST(push_received);
   BOOST_TEST(c1_called);
   BOOST_TEST(c2_called);
   BOOST_TEST(c3_called);
}

void test_push_received1()
{
   net::io_context ioc;
   auto conn = std::make_shared<connection>(ioc);

   flat_tree resp;
   conn->set_receive_response(resp);

   // Trick: Uses SUBSCRIBE because this command has no response or
   // better said, its response is a server push, which is what we
   // want to test.
   request req;
   req.push("SUBSCRIBE", "channel1");
   req.push("SUBSCRIBE", "channel2");

   bool push_received = false, exec_finished = false;

   conn->async_exec(req, ignore, [&, conn](error_code ec, std::size_t) {
      exec_finished = true;
      std::cout << "async_exec" << std::endl;
      BOOST_TEST_EQ(ec, error_code());
   });

   conn->async_receive2([&, conn](error_code ec) {
      push_received = true;
      std::cout << "async_receive2" << std::endl;

      BOOST_TEST_EQ(ec, error_code());
      BOOST_TEST_EQ(resp.get_total_msgs(), 2u);

      conn->cancel();
   });

   run(conn);
   ioc.run_for(test_timeout);

   BOOST_TEST(exec_finished);
   BOOST_TEST(push_received);
}

void test_push_filtered_out()
{
   net::io_context ioc;
   auto conn = std::make_shared<connection>(ioc);

   request req;
   req.push("HELLO", 3);
   req.push("PING");
   req.push("SUBSCRIBE", "channel");
   req.push("QUIT");

   response<ignore_t, std::string, std::string> resp;

   bool exec_finished = false, push_received = false;

   conn->async_exec(req, resp, [conn, &exec_finished](error_code ec, std::size_t) {
      exec_finished = true;
      BOOST_TEST_EQ(ec, error_code());
   });

   conn->async_receive2([&, conn](error_code ec) {
      push_received = true;
      BOOST_TEST_EQ(ec, error_code());
      conn->cancel(operation::reconnection);
   });

   run(conn);

   ioc.run_for(test_timeout);
   BOOST_TEST(exec_finished);
   BOOST_TEST(push_received);

   BOOST_TEST_EQ(std::get<1>(resp).value(), "PONG");
   BOOST_TEST_EQ(std::get<2>(resp).value(), "OK");
}

struct response_error_tag { };
response_error_tag error_tag_obj;

struct response_error_adapter {
   void on_init() { }
   void on_done() { }

   void on_node(
      boost::redis::resp3::basic_node<std::string_view> const&,
      boost::system::error_code& ec)
   {
      ec = boost::redis::error::incompatible_size;
   }
};

auto boost_redis_adapt(response_error_tag&) { return response_error_adapter{}; }

void test_test_push_adapter()
{
   net::io_context ioc;
   auto conn = std::make_shared<connection>(ioc);

   request req;
   req.push("HELLO", 3);
   req.push("PING");
   req.push("SUBSCRIBE", "channel");
   req.push("PING");

   conn->set_receive_response(error_tag_obj);

   bool push_received = false, exec_finished = false, run_finished = false;

   conn->async_receive2([&, conn](error_code ec) {
      BOOST_TEST_EQ(ec, net::experimental::error::channel_cancelled);
      push_received = true;
   });

   conn->async_exec(req, ignore, [&exec_finished](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
      exec_finished = true;
   });

   auto cfg = make_test_config();
   cfg.reconnect_wait_interval = 0s;
   conn->async_run(cfg, [&run_finished](error_code ec) {
      BOOST_TEST_EQ(ec, error::incompatible_size);
      run_finished = true;
   });

   ioc.run_for(test_timeout);
   BOOST_TEST(push_received);
   BOOST_TEST(exec_finished);
   BOOST_TEST(run_finished);

   // TODO: Reset the ioc reconnect and send a quit to ensure
   // reconnection is possible after an error.
}

void launch_push_consumer(std::shared_ptr<connection> conn)
{
   conn->async_receive2([conn](error_code ec) {
      if (ec) {
         BOOST_TEST_EQ(ec, net::experimental::error::channel_cancelled);
         return;
      }
      launch_push_consumer(conn);
   });
}

void test_many_subscribers()
{
   request req0;
   req0.get_config().cancel_on_connection_lost = false;
   req0.push("HELLO", 3);

   request req1;
   req1.get_config().cancel_on_connection_lost = false;
   req1.push("PING", "Message1");

   request req2;
   req2.get_config().cancel_on_connection_lost = false;
   req2.push("SUBSCRIBE", "channel");

   request req3;
   req3.get_config().cancel_on_connection_lost = false;
   req3.push("QUIT");

   net::io_context ioc;
   auto conn = std::make_shared<connection>(ioc);

   bool finished = false;

   auto c11 = [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, error_code());
      conn->cancel(operation::reconnection);
      finished = true;
   };
   auto c10 = [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, error_code());
      conn->async_exec(req3, ignore, c11);
   };
   auto c9 = [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, error_code());
      conn->async_exec(req2, ignore, c10);
   };
   auto c8 = [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, error_code());
      conn->async_exec(req1, ignore, c9);
   };
   auto c7 = [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, error_code());
      conn->async_exec(req2, ignore, c8);
   };
   auto c6 = [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, error_code());
      conn->async_exec(req2, ignore, c7);
   };
   auto c5 = [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, error_code());
      conn->async_exec(req1, ignore, c6);
   };
   auto c4 = [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, error_code());
      conn->async_exec(req2, ignore, c5);
   };
   auto c3 = [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, error_code());
      conn->async_exec(req1, ignore, c4);
   };
   auto c2 = [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, error_code());
      conn->async_exec(req2, ignore, c3);
   };
   auto c1 = [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, error_code());
      conn->async_exec(req2, ignore, c2);
   };
   auto c0 = [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, error_code());
      conn->async_exec(req1, ignore, c1);
   };

   conn->async_exec(req0, ignore, c0);
   launch_push_consumer(conn);

   run(conn, make_test_config(), {});

   ioc.run_for(test_timeout);
   BOOST_TEST(finished);
}

void test_test_unsubscribe()
{
   net::io_context ioc;
   connection conn{ioc};

   // Subscribe to 3 channels and 2 patterns. Use CLIENT INFO to verify this took effect
   request req_subscribe;
   req_subscribe.push("SUBSCRIBE", "ch1", "ch2", "ch3");
   req_subscribe.push("PSUBSCRIBE", "ch1*", "ch2*");
   req_subscribe.push("CLIENT", "INFO");

   // Then, unsubscribe from some of them, and verify again
   request req_unsubscribe;
   req_unsubscribe.push("UNSUBSCRIBE", "ch1");
   req_unsubscribe.push("PUNSUBSCRIBE", "ch2*");
   req_unsubscribe.push("CLIENT", "INFO");

   // Finally, ping to verify that the connection is still usable
   request req_ping;
   req_ping.push("PING", "test_unsubscribe");

   response<std::string> resp_subscribe, resp_unsubscribe, resp_ping;

   bool subscribe_finished = false, unsubscribe_finished = false, ping_finished = false,
        run_finished = false;

   auto on_ping = [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, error_code());
      ping_finished = true;
      BOOST_TEST(std::get<0>(resp_ping).has_value());
      BOOST_TEST_EQ(std::get<0>(resp_ping).value(), "test_unsubscribe");
      conn.cancel();
   };

   auto on_unsubscribe = [&](error_code ec, std::size_t) {
      unsubscribe_finished = true;
      BOOST_TEST_EQ(ec, error_code());
      BOOST_TEST(std::get<0>(resp_unsubscribe).has_value());
      BOOST_TEST_EQ(find_client_info(std::get<0>(resp_unsubscribe).value(), "sub"), "2");
      BOOST_TEST_EQ(find_client_info(std::get<0>(resp_unsubscribe).value(), "psub"), "1");
      conn.async_exec(req_ping, resp_ping, on_ping);
   };

   auto on_subscribe = [&](error_code ec, std::size_t) {
      subscribe_finished = true;
      BOOST_TEST_EQ(ec, error_code());
      BOOST_TEST(std::get<0>(resp_subscribe).has_value());
      BOOST_TEST_EQ(find_client_info(std::get<0>(resp_subscribe).value(), "sub"), "3");
      BOOST_TEST_EQ(find_client_info(std::get<0>(resp_subscribe).value(), "psub"), "2");
      conn.async_exec(req_unsubscribe, resp_unsubscribe, on_unsubscribe);
   };

   conn.async_exec(req_subscribe, resp_subscribe, on_subscribe);

   conn.async_run(make_test_config(), [&run_finished](error_code ec) {
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
      run_finished = true;
   });

   ioc.run_for(test_timeout);

   BOOST_TEST(subscribe_finished);
   BOOST_TEST(unsubscribe_finished);
   BOOST_TEST(ping_finished);
   BOOST_TEST(run_finished);
}

class test_pubsub_state_restoration {
   net::io_context ioc;
   connection conn{ioc};
   request req;
   response<std::string> resp_str;
   flat_tree resp_push;
   bool exec_finished = false;

   void check_subscriptions()
   {
      // Checks for the expected subscriptions and patterns after restoration
      std::set<std::string_view> seen_channels, seen_patterns;
      for (auto it = resp_push.get_view().begin(); it != resp_push.get_view().end();) {
         // The root element should be a push
         BOOST_TEST_EQ(it->data_type, type::push);
         BOOST_TEST_GE(it->aggregate_size, 2u);
         BOOST_TEST(++it != resp_push.get_view().end());

         // The next element should be the message type
         std::string_view msg_type = it->value;
         BOOST_TEST(++it != resp_push.get_view().end());

         // The next element is the channel or pattern
         if (msg_type == "subscribe")
            seen_channels.insert(it->value);
         else if (msg_type == "psubscribe")
            seen_patterns.insert(it->value);

         // Skip the rest of the nodes
         while (it != resp_push.get_view().end() && it->depth != 0u)
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

   void sub1()
   {
      // Subscribe to some channels and patterns
      req.clear();
      req.subscribe({"ch1", "ch2", "ch3"});              // active: 1, 2, 3
      req.psubscribe({"ch1*", "ch2*", "ch3*", "ch4*"});  // active: 1, 2, 3, 4
      conn.async_exec(req, ignore, [this](error_code ec, std::size_t) {
         BOOST_TEST_EQ(ec, error_code());
         unsub();
      });
   }

   void unsub()
   {
      // Unsubscribe from some channels and patterns.
      // Unsubscribing from a channel/pattern that we weren't subscribed to is OK.
      req.clear();
      req.unsubscribe({"ch2", "ch1", "ch5"});      // active: 3
      req.punsubscribe({"ch2*", "ch4*", "ch9*"});  // active: 1, 3
      conn.async_exec(req, ignore, [this](error_code ec, std::size_t) {
         BOOST_TEST_EQ(ec, error_code());
         sub2();
      });
   }

   void sub2()
   {
      // Subscribe to other channels/patterns.
      // Re-subscribing to channels/patterns we unsubscribed from is OK.
      // Subscribing to the same channel/pattern twice is OK.
      req.clear();
      req.subscribe({"ch1", "ch3", "ch5"});      // active: 1, 3, 5
      req.psubscribe({"ch3*", "ch4*", "ch8*"});  // active: 1, 3, 4, 8

      // Subscriptions created by push() don't survive reconnection
      req.push("SUBSCRIBE", "ch10");    // active: 1, 3, 5, 10
      req.push("PSUBSCRIBE", "ch10*");  // active: 1, 3, 4, 8, 10

      // Validate that we're subscribed to what we expect
      req.push("CLIENT", "INFO");

      conn.async_exec(req, resp_str, [this](error_code ec, std::size_t) {
         BOOST_TEST_EQ(ec, error_code());

         // We are subscribed to 4 channels and 5 patterns
         BOOST_TEST(std::get<0>(resp_str).has_value());
         BOOST_TEST_EQ(find_client_info(std::get<0>(resp_str).value(), "sub"), "4");
         BOOST_TEST_EQ(find_client_info(std::get<0>(resp_str).value(), "psub"), "5");

         resp_push.clear();

         quit();
      });
   }

   void quit()
   {
      req.clear();
      req.push("QUIT");

      conn.async_exec(req, ignore, [this](error_code, std::size_t) {
         // we don't know if this request will complete successfully or not
         client_info();
      });
   }

   void client_info()
   {
      req.clear();
      req.push("CLIENT", "INFO");
      req.get_config().cancel_if_unresponded = false;

      conn.async_exec(req, resp_str, [this](error_code ec, std::size_t) {
         BOOST_TEST_EQ(ec, error_code());

         // We are subscribed to 3 channels and 4 patterns (1 of each didn't survive reconnection)
         BOOST_TEST(std::get<0>(resp_str).has_value());
         BOOST_TEST_EQ(find_client_info(std::get<0>(resp_str).value(), "sub"), "3");
         BOOST_TEST_EQ(find_client_info(std::get<0>(resp_str).value(), "psub"), "4");

         // We have received pushes confirming it
         check_subscriptions();

         exec_finished = true;
         conn.cancel();
      });
   }

public:
   void run()
   {
      conn.set_receive_response(resp_push);

      // Start the request chain
      sub1();

      // Start running
      bool run_finished = false;
      conn.async_run(make_test_config(), [&run_finished](error_code ec) {
         BOOST_TEST_EQ(ec, net::error::operation_aborted);
         run_finished = true;
      });

      ioc.run_for(test_timeout);

      // Done
      BOOST_TEST(exec_finished);
      BOOST_TEST(run_finished);
   }
};

}  // namespace

int main()
{
   test_receives_push_waiting_resps();
   test_push_received1();
   test_push_filtered_out();
   test_test_push_adapter();
   test_many_subscribers();
   test_test_unsubscribe();
   test_pubsub_state_restoration{}.run();

   return boost::report_errors();
}
