/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>
#include <boost/redis/logger.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/resp3/flat_tree.hpp>
#include <boost/redis/resp3/node.hpp>
#include <boost/redis/response.hpp>

#include <boost/asio/error.hpp>
#include <boost/asio/experimental/channel_error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/core/lightweight_test.hpp>

#include "common.hpp"

#include <cstddef>
#include <string>

namespace net = boost::asio;
using namespace boost::redis;
using namespace std::chrono_literals;
using boost::system::error_code;

namespace {

// async_receive is outstanding when a push is received
void test_async_receive_waiting_for_push()
{
   resp3::flat_tree resp;
   net::io_context ioc;
   connection conn{ioc};
   conn.set_receive_response(resp);

   request req1;
   req1.push("PING", "Message1");
   req1.push("SUBSCRIBE", "test_async_receive_waiting_for_push");

   request req2;
   req2.push("PING", "Message2");

   bool run_finished = false, push_received = false, exec1_finished = false, exec2_finished = false;

   auto on_exec2 = [&](error_code ec2, std::size_t) {
      BOOST_TEST_EQ(ec2, error_code());
      exec2_finished = true;
      conn.cancel();
   };

   conn.async_exec(req1, ignore, [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, error_code());
      exec1_finished = true;
   });

   conn.async_receive([&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, error_code());
      BOOST_TEST_EQ(resp.get_total_msgs(), 1u);
      push_received = true;
      conn.async_exec(req2, ignore, on_exec2);
   });

   conn.async_run(make_test_config(), [&](error_code ec) {
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
      run_finished = true;
   });

   ioc.run_for(test_timeout);

   BOOST_TEST(push_received);
   BOOST_TEST(exec1_finished);
   BOOST_TEST(exec2_finished);
   BOOST_TEST(run_finished);
}

// A push is already available when async_receive is called
void test_async_receive_push_available()
{
   net::io_context ioc;
   connection conn{ioc};
   resp3::flat_tree resp;
   conn.set_receive_response(resp);

   // SUBSCRIBE doesn't have a response, but causes a push to be delivered.
   // Add a PING so the overall request has a response.
   // This ensures that when async_exec completes, the push has been delivered
   request req;
   req.push("SUBSCRIBE", "test_async_receive_push_available");
   req.push("PING", "message");

   bool push_received = false, exec_finished = false, run_finished = false;

   auto on_receive = [&](error_code ec, std::size_t) {
      push_received = true;
      BOOST_TEST_EQ(ec, error_code());
      BOOST_TEST_EQ(resp.get_total_msgs(), 1u);
      conn.cancel();
   };

   conn.async_exec(req, ignore, [&](error_code ec, std::size_t) {
      exec_finished = true;
      BOOST_TEST_EQ(ec, error_code());
      conn.async_receive(on_receive);
   });

   conn.async_run(make_test_config(), [&](error_code ec) {
      run_finished = true;
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
   });

   ioc.run_for(test_timeout);

   BOOST_TEST(exec_finished);
   BOOST_TEST(push_received);
   BOOST_TEST(run_finished);
}

// Synchronous receive can be used to try to read a message
void test_sync_receive()
{
   net::io_context ioc;
   connection conn{ioc};
   resp3::flat_tree resp;
   conn.set_receive_response(resp);

   // Subscribing to 2 channels causes 2 pushes to be delivered.
   // Adding a PING guarantees that after exec finishes, the push has been read
   request req;
   req.push("SUBSCRIBE", "test_sync_receive_channel1");
   req.push("SUBSCRIBE", "test_sync_receive_channel2");
   req.push("PING", "message");

   bool exec_finished = false, run_finished = false;

   conn.async_exec(req, ignore, [&](error_code ec, std::size_t) {
      exec_finished = true;
      BOOST_TEST_EQ(ec, error_code());

      // At this point, the receive response contains all the pushes
      BOOST_TEST_EQ(resp.get_total_msgs(), 2u);

      // Receive the 1st push synchronously
      std::size_t push_bytes = conn.receive(ec);
      BOOST_TEST_EQ(ec, error_code());
      BOOST_TEST_GT(push_bytes, 0u);

      // Receive the 2nd push synchronously
      push_bytes = conn.receive(ec);
      BOOST_TEST_EQ(ec, error_code());
      BOOST_TEST_GT(push_bytes, 0u);

      // There are no more pushes. Trying to receive one more fails
      push_bytes = conn.receive(ec);
      BOOST_TEST_EQ(ec, error::sync_receive_push_failed);
      BOOST_TEST_EQ(push_bytes, 0u);

      conn.cancel();
   });

   conn.async_run(make_test_config(), [&](error_code ec) {
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
      run_finished = true;
   });

   // Trying to receive a push before one is received fails
   error_code ec;
   std::size_t push_bytes = conn.receive(ec);
   BOOST_TEST_EQ(ec, error::sync_receive_push_failed);
   BOOST_TEST_EQ(push_bytes, 0u);

   ioc.run_for(test_timeout);

   BOOST_TEST(exec_finished);
   BOOST_TEST(run_finished);
}

// A push may be interleaved between regular responses.
// It is handed to the receive adapter (filtered out).
void test_exec_push_interleaved()
{
   net::io_context ioc;
   connection conn{ioc};
   resp3::flat_tree receive_resp;
   conn.set_receive_response(receive_resp);

   request req;
   req.push("PING", "msg1");
   req.push("SUBSCRIBE", "test_exec_push_interleaved");
   req.push("PING", "msg2");

   response<std::string, std::string> resp;

   bool exec_finished = false, push_received = false, run_finished = false;

   conn.async_exec(req, resp, [&](error_code ec, std::size_t) {
      exec_finished = true;
      BOOST_TEST_EQ(ec, error_code());
      BOOST_TEST_EQ(std::get<0>(resp).value(), "msg1");
      BOOST_TEST_EQ(std::get<1>(resp).value(), "msg2");
      conn.cancel();
   });

   conn.async_receive([&](error_code ec, std::size_t) {
      push_received = true;
      BOOST_TEST_EQ(ec, error_code());
      BOOST_TEST_EQ(receive_resp.get_total_msgs(), 1u);
   });

   conn.async_run(make_test_config(), [&](error_code ec) {
      run_finished = true;
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
   });

   ioc.run_for(test_timeout);

   BOOST_TEST(exec_finished);
   BOOST_TEST(push_received);
   BOOST_TEST(run_finished);
}

// An adapter that always errors
struct response_error_tag { };
response_error_tag error_tag_obj;

struct response_error_adapter {
   void on_init() { }
   void on_done() { }
   void on_node(resp3::node_view const&, error_code& ec) { ec = error::incompatible_size; }
};

auto boost_redis_adapt(response_error_tag&) { return response_error_adapter{}; }

// If the push adapter returns an error, the connection is torn down
// (a reconnection would be triggered)
// TODO: this test should be in push2
void test_push_adapter_error()
{
   net::io_context ioc;
   connection conn{ioc};
   conn.set_receive_response(error_tag_obj);

   request req;
   req.push("PING");
   req.push("SUBSCRIBE", "channel");
   req.push("PING");

   bool push_received = false, exec_finished = false, run_finished = false;

   // async_receive is cancelled every reconnection cycle
   conn.async_receive([&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, net::experimental::error::channel_cancelled);
      push_received = true;
   });

   // The request is cancelled because the PING response isn't processed
   // by the time the error is generated
   conn.async_exec(req, ignore, [&exec_finished](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
      exec_finished = true;
   });

   auto cfg = make_test_config();
   cfg.reconnect_wait_interval = 0s;  // so we can validate the generated error
   conn.async_run(cfg, [&run_finished](error_code ec) {
      BOOST_TEST_EQ(ec, error::incompatible_size);
      run_finished = true;
   });

   ioc.run_for(test_timeout);
   BOOST_TEST(push_received);
   BOOST_TEST(exec_finished);
   BOOST_TEST(run_finished);
}

// TODO: push adapter errors trigger a reconnection
// TODO: async_receive is cancelled when a reconnection happens

// After an async_receive operation finishes, another one can be issued
struct test_consecutive_receives {
   net::io_context ioc;
   connection conn{ioc};
   resp3::flat_tree resp;
   bool push_consumer_finished{false};

   void launch_push_consumer()
   {
      conn.async_receive([this](error_code ec, std::size_t) {
         if (ec) {
            BOOST_TEST_EQ(ec, net::experimental::error::channel_cancelled);
            push_consumer_finished = true;
            resp.clear();
            return;
         }
         launch_push_consumer();
      });
   }

   void run()
   {
      conn.set_receive_response(resp);

      request req1;
      req1.get_config().cancel_on_connection_lost = false;
      req1.push("PING", "Message1");

      request req2;
      req2.get_config().cancel_on_connection_lost = false;
      req2.push("SUBSCRIBE", "channel");

      bool exec_finished = false, run_finished = false;

      auto c10 = [&](error_code ec, std::size_t) {
         BOOST_TEST_EQ(ec, error_code());
         exec_finished = true;
         conn.cancel();
      };
      auto c9 = [&](error_code ec, std::size_t) {
         BOOST_TEST_EQ(ec, error_code());
         conn.async_exec(req2, ignore, c10);
      };
      auto c8 = [&](error_code ec, std::size_t) {
         BOOST_TEST_EQ(ec, error_code());
         conn.async_exec(req1, ignore, c9);
      };
      auto c7 = [&](error_code ec, std::size_t) {
         BOOST_TEST_EQ(ec, error_code());
         conn.async_exec(req2, ignore, c8);
      };
      auto c6 = [&](error_code ec, std::size_t) {
         BOOST_TEST_EQ(ec, error_code());
         conn.async_exec(req2, ignore, c7);
      };
      auto c5 = [&](error_code ec, std::size_t) {
         BOOST_TEST_EQ(ec, error_code());
         conn.async_exec(req1, ignore, c6);
      };
      auto c4 = [&](error_code ec, std::size_t) {
         BOOST_TEST_EQ(ec, error_code());
         conn.async_exec(req2, ignore, c5);
      };
      auto c3 = [&](error_code ec, std::size_t) {
         BOOST_TEST_EQ(ec, error_code());
         conn.async_exec(req1, ignore, c4);
      };
      auto c2 = [&](error_code ec, std::size_t) {
         BOOST_TEST_EQ(ec, error_code());
         conn.async_exec(req2, ignore, c3);
      };
      auto c1 = [&](error_code ec, std::size_t) {
         BOOST_TEST_EQ(ec, error_code());
         conn.async_exec(req2, ignore, c2);
      };

      conn.async_exec(req1, ignore, c1);
      launch_push_consumer();

      conn.async_run(make_test_config(), [&](error_code ec) {
         run_finished = true;
         BOOST_TEST_EQ(ec, net::error::operation_aborted);
      });

      ioc.run_for(test_timeout);

      BOOST_TEST(exec_finished);
      BOOST_TEST(run_finished);
      BOOST_TEST(push_consumer_finished);
   }
};

void test_unsubscribe()
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
      BOOST_TEST(std::get<0>(resp_ping).value() == "test_unsubscribe");
      conn.cancel();
   };

   auto on_unsubscribe = [&](error_code ec, std::size_t) {
      unsubscribe_finished = true;
      BOOST_TEST_EQ(ec, error_code());
      BOOST_TEST(std::get<0>(resp_unsubscribe).has_value());
      BOOST_TEST(find_client_info(std::get<0>(resp_unsubscribe).value(), "sub") == "2");
      BOOST_TEST(find_client_info(std::get<0>(resp_unsubscribe).value(), "psub") == "1");
      conn.async_exec(req_ping, resp_ping, on_ping);
   };

   auto on_subscribe = [&](error_code ec, std::size_t) {
      subscribe_finished = true;
      BOOST_TEST_EQ(ec, error_code());
      BOOST_TEST(std::get<0>(resp_subscribe).has_value());
      BOOST_TEST(find_client_info(std::get<0>(resp_subscribe).value(), "sub") == "3");
      BOOST_TEST(find_client_info(std::get<0>(resp_subscribe).value(), "psub") == "2");
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

}  // namespace

int main()
{
   test_async_receive_waiting_for_push();
   test_async_receive_push_available();
   test_sync_receive();
   test_exec_push_interleaved();
   test_push_adapter_error();
   test_consecutive_receives{}.run();
   test_unsubscribe();

   return boost::report_errors();
}
