/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/adapter/any_adapter.hpp>
#include <boost/redis/detail/multiplexer.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/resp3/node.hpp>
#include <boost/redis/resp3/serialization.hpp>
#include <boost/redis/resp3/type.hpp>
#include <boost/redis/response.hpp>

#include <boost/core/lightweight_test.hpp>

#include <iostream>
#include <iterator>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>

using boost::redis::request;
using boost::redis::detail::multiplexer;
using boost::redis::detail::consume_result;
using boost::redis::generic_response;
using boost::redis::resp3::node;
using boost::redis::resp3::to_string;
using boost::redis::resp3::type;
using boost::redis::response;
using boost::redis::any_adapter;
using boost::system::error_code;

namespace boost::redis::resp3 {

std::ostream& operator<<(std::ostream& os, node const& nd)
{
   return os << "node{ .data_type=" << to_string(nd.data_type)
             << ", .aggregate_size=" << nd.aggregate_size << ", .depth=" << nd.depth
             << ", .value=" << nd.value << "}";
}

}  // namespace boost::redis::resp3

namespace boost::redis::detail {

std::ostream& operator<<(std::ostream& os, consume_result v)
{
   switch (v) {
      case consume_result::needs_more:   return os << "consume_result::needs_more";
      case consume_result::got_response: return os << "consume_result::got_response";
      case consume_result::got_push:     return os << "consume_result::got_push";
      default:                           return os << "<unknown consume_result>";
   }
}

}  // namespace boost::redis::detail

namespace {

struct test_item {
   request req;
   generic_response resp;
   std::shared_ptr<multiplexer::elem> elem_ptr;
   bool done = false;

   test_item(bool cmd_with_response = true)
   {
      // The exact command is irrelevant because it is not being sent
      // to Redis.
      req.push(cmd_with_response ? "PING" : "SUBSCRIBE", "cmd-arg");

      elem_ptr = std::make_shared<multiplexer::elem>(req, any_adapter{resp});

      elem_ptr->set_done_callback([this]() {
         done = true;
      });
   }
};

void test_request_needs_more()
{
   // Setup
   test_item item1{};
   multiplexer mpx;

   // Add the element to the multiplexer and simulate a successful write
   mpx.add(item1.elem_ptr);
   BOOST_TEST_EQ(mpx.prepare_write(), 1u);
   BOOST_TEST_EQ(mpx.commit_write(), 0u);
   BOOST_TEST(item1.elem_ptr->is_written());
   BOOST_TEST(!item1.done);

   // Parse part of the response
   error_code ec;
   auto ret = mpx.consume_next("$11\r\nhello", ec);
   BOOST_TEST_EQ(ret.first, consume_result::needs_more);

   // Parse the rest of it
   ret = mpx.consume_next("$11\r\nhello world\r\n", ec);
   BOOST_TEST_EQ(ret.first, consume_result::got_response);
   BOOST_TEST(item1.resp.has_value());

   const node expected[] = {
      {type::blob_string, 1u, 0u, "hello world"},
   };
   BOOST_TEST_ALL_EQ(
      item1.resp->begin(),
      item1.resp->end(),
      std::begin(expected),
      std::end(expected));
}

void test_several_requests()
{
   test_item item1{};
   test_item item2{false};
   test_item item3{};

   // Add some requests to the multiplexer.
   multiplexer mpx;
   mpx.add(item1.elem_ptr);
   mpx.add(item3.elem_ptr);
   mpx.add(item2.elem_ptr);

   // These requests haven't been written yet so their statuses should
   // be "waiting.".
   BOOST_TEST(item1.elem_ptr->is_waiting());
   BOOST_TEST(item2.elem_ptr->is_waiting());
   BOOST_TEST(item3.elem_ptr->is_waiting());

   // There are three requests to coalesce, a second call should do
   // nothing.
   BOOST_TEST_EQ(mpx.prepare_write(), 3u);
   BOOST_TEST_EQ(mpx.prepare_write(), 0u);

   // After coalescing the requests for writing their statuses should
   // be changed to "staged".
   BOOST_TEST(item1.elem_ptr->is_staged());
   BOOST_TEST(item2.elem_ptr->is_staged());
   BOOST_TEST(item3.elem_ptr->is_staged());

   // There are no waiting requests to cancel since they are all
   // staged.
   BOOST_TEST_EQ(mpx.cancel_waiting(), 0u);

   // Since the requests haven't been sent (written) the done
   // callback should not have been called yet.
   BOOST_TEST(!item1.done);
   BOOST_TEST(!item2.done);
   BOOST_TEST(!item3.done);

   // The commit_write call informs the multiplexer the payload was
   // sent (e.g.  written to the socket). This step releases requests
   // that has no response.
   BOOST_TEST_EQ(mpx.commit_write(), 1u);

   // The staged status should now have changed to written.
   BOOST_TEST(item1.elem_ptr->is_written());
   BOOST_TEST(item2.elem_ptr->is_done());
   BOOST_TEST(item3.elem_ptr->is_written());

   // The done status should still be unchanged on requests that
   // expect a response.
   BOOST_TEST(!item1.done);
   BOOST_TEST(item2.done);
   BOOST_TEST(!item3.done);

   // Consumes the next message in the read buffer.
   error_code ec;
   auto ret = mpx.consume_next("+one\r\n", ec);

   // The read operation should have been successful.
   BOOST_TEST_EQ(ret.first, consume_result::got_response);
   BOOST_TEST(ret.second != 0u);

   // The last request still did not get a response.
   BOOST_TEST(item1.done);
   BOOST_TEST(item2.done);
   BOOST_TEST(!item3.done);

   // Consumes the second message in the read buffer
   // Consumes the next message in the read buffer.
   ret = mpx.consume_next("+two\r\n", ec);

   // The read operation should have been successful.
   BOOST_TEST_EQ(ret.first, consume_result::got_response);
   BOOST_TEST(ret.second != 0u);

   // Everything done
   BOOST_TEST(item1.done);
   BOOST_TEST(item2.done);
   BOOST_TEST(item3.done);
}

// The response to a request is received before its write
// confirmation. This might happen on heavy load
void test_request_response_before_write()
{
   // Setup
   auto item = std::make_unique<test_item>();
   multiplexer mpx;

   // Add the request and trigger the write
   mpx.add(item->elem_ptr);
   BOOST_TEST(item->elem_ptr->is_waiting());
   BOOST_TEST_EQ(mpx.prepare_write(), 1u);
   BOOST_TEST(item->elem_ptr->is_staged());
   BOOST_TEST(!item->done);

   // The response is received. The request is marked as done,
   // even if the write hasn't been confirmed yet
   error_code ec;
   auto ret = mpx.consume_next("+one\r\n", ec);

   BOOST_TEST_EQ(ret.first, consume_result::got_response);
   BOOST_TEST_EQ(ec, error_code());
   BOOST_TEST(item->done);

   // The request is removed
   item.reset();

   // The write gets confirmed and causes no problem
   BOOST_TEST_EQ(mpx.commit_write(), 0u);
}

void test_push()
{
   // Setup
   multiplexer mpx;
   generic_response resp;
   mpx.set_receive_adapter(any_adapter{resp});

   // Consume an entire push
   error_code ec;
   auto const ret = mpx.consume_next(">2\r\n+one\r\n+two\r\n", ec);

   // Check
   BOOST_TEST_EQ(ret.first, consume_result::got_push);
   BOOST_TEST_EQ(ret.second, 16u);
   BOOST_TEST(resp.has_value());

   const node expected[] = {
      {type::push,          2u, 0u, ""   },
      {type::simple_string, 1u, 1u, "one"},
      {type::simple_string, 1u, 1u, "two"},
   };
   BOOST_TEST_ALL_EQ(resp->begin(), resp->end(), std::begin(expected), std::end(expected));
}

void test_push_needs_more()
{
   // Setup
   multiplexer mpx;
   generic_response resp;
   mpx.set_receive_adapter(any_adapter{resp});
   std::string msg;

   // Only part of the message available.
   msg += ">2\r\n+one\r";

   // Consume it
   error_code ec;
   auto ret = mpx.consume_next(msg, ec);

   BOOST_TEST_EQ(ret.first, consume_result::needs_more);

   // The entire message becomes available
   msg += "\n+two\r\n";
   ret = mpx.consume_next(msg, ec);

   BOOST_TEST_EQ(ret.first, consume_result::got_push);
   BOOST_TEST_EQ(ret.second, 16u);
   BOOST_TEST(resp.has_value());

   const node expected[] = {
      {type::push,          2u, 0u, ""   },
      {type::simple_string, 1u, 1u, "one"},
      {type::simple_string, 1u, 1u, "two"},
   };
   BOOST_TEST_ALL_EQ(resp->begin(), resp->end(), std::begin(expected), std::end(expected));
}

// If a response is received and no request is waiting, it is interpreted
// as a push (e.g. MONITOR)
void test_push_heuristics_no_request()
{
   // Setup
   multiplexer mpx;
   generic_response resp;
   mpx.set_receive_adapter(any_adapter{resp});

   // Response received, but no request has been sent
   error_code ec;
   auto const ret = mpx.consume_next("+Hello world\r\n", ec);

   // Check
   BOOST_TEST_EQ(ret.first, consume_result::got_push);
   BOOST_TEST_EQ(ret.second, 14u);
   BOOST_TEST(resp.has_value());

   const node expected[] = {
      {type::simple_string, 1u, 0u, "Hello world"},
   };
   BOOST_TEST_ALL_EQ(resp->begin(), resp->end(), std::begin(expected), std::end(expected));
}

// Same, but there's another request that hasn't been written yet.
// This is an edge case but might happen due to race conditions.
void test_push_heuristics_request_waiting()
{
   // Setup
   multiplexer mpx;
   generic_response resp;
   mpx.set_receive_adapter(any_adapter{resp});
   test_item item;

   // Add the item but don't write it (e.g. the writer task is busy)
   mpx.add(item.elem_ptr);

   // Response received, but no request has been sent
   error_code ec;
   auto const ret = mpx.consume_next("+Hello world\r\n", ec);

   // Check
   BOOST_TEST_EQ(ret.first, consume_result::got_push);
   BOOST_TEST_EQ(ret.second, 14u);
   BOOST_TEST(resp.has_value());

   const node expected[] = {
      {type::simple_string, 1u, 0u, "Hello world"},
   };
   BOOST_TEST_ALL_EQ(resp->begin(), resp->end(), std::begin(expected), std::end(expected));
}

// If a response is received and the first request doesn't expect a response,
// it is interpreted as a push (e.g. SUBSCRIBE with incorrect syntax)
void test_push_heuristics_request_without_response()
{
   // Setup
   multiplexer mpx;
   generic_response resp;
   mpx.set_receive_adapter(any_adapter{resp});
   test_item item{false};

   // Add the request to the multiplexer
   mpx.add(item.elem_ptr);

   // Write it, but don't confirm the write, so the request doesn't get removed
   BOOST_TEST_EQ(mpx.prepare_write(), 1u);

   // Response received (e.g. syntax error)
   error_code ec;
   auto const ret = mpx.consume_next("-ERR wrong syntax\r\n", ec);

   // Check
   BOOST_TEST_EQ(ret.first, consume_result::got_push);
   BOOST_TEST_EQ(ret.second, 19u);
   BOOST_TEST_EQ(resp.error().diagnostic, "ERR wrong syntax");
}

// We correctly reset parsing state between requests and pushes.
void test_mix_responses_pushes()
{
   // Setup
   multiplexer mpx;
   generic_response push_resp;
   mpx.set_receive_adapter(any_adapter{push_resp});
   test_item item1, item2;

   // Add the elements to the multiplexer and simulate a successful write
   mpx.add(item1.elem_ptr);
   mpx.add(item2.elem_ptr);
   BOOST_TEST_EQ(mpx.prepare_write(), 2u);
   BOOST_TEST_EQ(mpx.commit_write(), 0u);
   BOOST_TEST(item1.elem_ptr->is_written());
   BOOST_TEST(!item1.done);
   BOOST_TEST(item2.elem_ptr->is_written());
   BOOST_TEST(!item2.done);

   // Push
   std::string_view push1_buffer = ">2\r\n+one\r\n+two\r\n";
   error_code ec;
   auto ret = mpx.consume_next(push1_buffer, ec);
   BOOST_TEST_EQ(ret.first, consume_result::got_push);
   BOOST_TEST_EQ(ret.second, 16u);
   BOOST_TEST(push_resp.has_value());
   std::vector<node> expected{
      {type::push,          2u, 0u, ""   },
      {type::simple_string, 1u, 1u, "one"},
      {type::simple_string, 1u, 1u, "two"},
   };
   BOOST_TEST_ALL_EQ(push_resp->begin(), push_resp->end(), expected.begin(), expected.end());
   BOOST_TEST_NOT(item1.done);
   BOOST_TEST_NOT(item2.done);

   // First response
   std::string_view response1_buffer = "$11\r\nHello world\r\n";
   ret = mpx.consume_next(response1_buffer, ec);
   BOOST_TEST_EQ(ret.first, consume_result::got_response);
   BOOST_TEST_EQ(ret.second, 18u);
   BOOST_TEST(item1.resp.has_value());
   expected = {
      {type::blob_string, 1u, 0u, "Hello world"},
   };
   BOOST_TEST_ALL_EQ(item1.resp->begin(), item1.resp->end(), expected.begin(), expected.end());
   BOOST_TEST(item1.done);
   BOOST_TEST_NOT(item2.done);

   // Push
   std::string_view push2_buffer = ">2\r\n+other\r\n+push\r\n";
   ret = mpx.consume_next(push2_buffer, ec);
   BOOST_TEST_EQ(ret.first, consume_result::got_push);
   BOOST_TEST_EQ(ret.second, 19u);
   BOOST_TEST(push_resp.has_value());
   expected = {
      {type::push,          2u, 0u, ""     },
      {type::simple_string, 1u, 1u, "one"  },
      {type::simple_string, 1u, 1u, "two"  },
      {type::push,          2u, 0u, ""     },
      {type::simple_string, 1u, 1u, "other"},
      {type::simple_string, 1u, 1u, "push" },
   };
   BOOST_TEST_ALL_EQ(push_resp->begin(), push_resp->end(), expected.begin(), expected.end());
   BOOST_TEST(item1.done);
   BOOST_TEST_NOT(item2.done);

   // Second response
   std::string_view response2_buffer = "$8\r\nResponse\r\n";
   ret = mpx.consume_next(response2_buffer, ec);
   BOOST_TEST_EQ(ret.first, consume_result::got_response);
   BOOST_TEST_EQ(ret.second, 14u);
   BOOST_TEST(item2.resp.has_value());
   expected = {
      {type::blob_string, 1u, 0u, "Response"},
   };
   BOOST_TEST_ALL_EQ(item2.resp->begin(), item2.resp->end(), expected.begin(), expected.end());
   BOOST_TEST(item1.done);
   BOOST_TEST(item2.done);

   // Check usage
   const auto usg = mpx.get_usage();
   BOOST_TEST_EQ(usg.commands_sent, 2u);
   BOOST_TEST_EQ(usg.bytes_sent, item1.req.payload().size() + item2.req.payload().size());
   BOOST_TEST_EQ(usg.responses_received, 2u);
   BOOST_TEST_EQ(usg.pushes_received, 2u);
   BOOST_TEST_EQ(usg.response_bytes_received, response1_buffer.size() + response2_buffer.size());
   BOOST_TEST_EQ(usg.push_bytes_received, push1_buffer.size() + push2_buffer.size());
}

// Cancellation on connection lost
void test_cancel_on_connection_lost()
{
   // Setup
   multiplexer mpx;
   test_item item_written1, item_written2, item_staged1, item_staged2, item_waiting1, item_waiting2;

   // Different items have different configurations
   // (note that these are all true by default)
   item_written1.req.get_config().cancel_if_unresponded = false;
   item_staged1.req.get_config().cancel_if_unresponded = false;
   item_waiting1.req.get_config().cancel_on_connection_lost = false;

   // Make each item reach the state it should be in
   mpx.add(item_written1.elem_ptr);
   mpx.add(item_written2.elem_ptr);
   BOOST_TEST_EQ(mpx.prepare_write(), 2u);
   BOOST_TEST_EQ(mpx.commit_write(), 0u);

   mpx.add(item_staged1.elem_ptr);
   mpx.add(item_staged2.elem_ptr);
   BOOST_TEST_EQ(mpx.prepare_write(), 2u);

   mpx.add(item_waiting1.elem_ptr);
   mpx.add(item_waiting2.elem_ptr);

   // Check that we got it right
   BOOST_TEST(item_written1.elem_ptr->is_written());
   BOOST_TEST(item_written2.elem_ptr->is_written());
   BOOST_TEST(item_staged1.elem_ptr->is_staged());
   BOOST_TEST(item_staged2.elem_ptr->is_staged());
   BOOST_TEST(item_waiting1.elem_ptr->is_waiting());
   BOOST_TEST(item_waiting2.elem_ptr->is_waiting());

   // Trigger a connection lost event
   mpx.cancel_on_conn_lost();

   // The ones with the cancellation settings set to false are back to waiting.
   // Others are cancelled
   BOOST_TEST(!item_written1.done);
   BOOST_TEST(item_written1.elem_ptr->is_waiting());
   BOOST_TEST(item_written2.done);
   BOOST_TEST(!item_staged1.done);
   BOOST_TEST(item_staged1.elem_ptr->is_waiting());
   BOOST_TEST(item_staged2.done);
   BOOST_TEST(!item_waiting1.done);
   BOOST_TEST(item_waiting1.elem_ptr->is_waiting());
   BOOST_TEST(item_waiting2.done);
}

// The test below fails. Uncomment when this is fixed:
// https://github.com/boostorg/redis/issues/307
// void test_cancel_on_connection_lost_half_parsed_response()
// {
//    // Setup
//    multiplexer mpx;
//    test_item item;
//    item.req.get_config().cancel_if_unresponded = false;
//    error_code ec;

//    // Add the request, write it and start parsing the response
//    mpx.add(item.elem_ptr);
//    BOOST_TEST_EQ(mpx.prepare_write(), 1u);
//    BOOST_TEST_EQ(mpx.commit_write(), 0u);
//    auto res = mpx.consume_next("*2\r\n+hello\r\n", ec);
//    BOOST_TEST_EQ(res.first, consume_result::needs_more);
//    BOOST_TEST_EQ(ec, error_code());

//    // Trigger a connection lost event
//    mpx.cancel_on_conn_lost();
//    BOOST_TEST(!item.done);
//    BOOST_TEST(item.elem_ptr->is_waiting());

//    // Simulate a reconnection
//    mpx.reset();

//    // Successful write, and this time the response is complete
//    BOOST_TEST_EQ(mpx.prepare_write(), 1u);
//    BOOST_TEST_EQ(mpx.commit_write(), 0u);
//    res = mpx.consume_next("*2\r\n+hello\r\n+world\r\n", ec);
//    BOOST_TEST_EQ(res.first, consume_result::got_response);
//    BOOST_TEST_EQ(ec, error_code());

//    // Check the response
//    BOOST_TEST(item.resp.has_value());
//    const node expected[] = {
//       {type::array,         2u, 0u, ""     },
//       {type::simple_string, 1u, 1u, "hello"},
//       {type::simple_string, 1u, 1u, "world"},
//    };
//    BOOST_TEST_ALL_EQ(
//       item.resp->begin(),
//       item.resp->end(),
//       std::begin(expected),
//       std::end(expected));
// }

// Resetting works
void test_reset()
{
   // Setup
   multiplexer mpx;
   generic_response push_resp;
   mpx.set_receive_adapter(any_adapter{push_resp});
   test_item item1, item2;

   // Add a request
   mpx.add(item1.elem_ptr);

   // Start parsing a push
   error_code ec;
   auto ret = mpx.consume_next(">2\r", ec);
   BOOST_TEST_EQ(ret.first, consume_result::needs_more);

   // Connection lost. The first request gets cancelled
   mpx.cancel_on_conn_lost();
   BOOST_TEST(item1.done);

   // Reconnection happens
   mpx.reset();
   ec.clear();

   // We're able to add write requests and read responses - all state was reset
   mpx.add(item2.elem_ptr);
   BOOST_TEST_EQ(mpx.prepare_write(), 1u);
   BOOST_TEST_EQ(mpx.commit_write(), 0u);

   std::string_view response_buffer = "$11\r\nHello world\r\n";
   ret = mpx.consume_next(response_buffer, ec);
   BOOST_TEST_EQ(ret.first, consume_result::got_response);
   BOOST_TEST_EQ(ret.second, response_buffer.size());
   BOOST_TEST(item2.resp.has_value());
   const node expected[] = {
      {type::blob_string, 1u, 0u, "Hello world"},
   };
   BOOST_TEST_ALL_EQ(
      item2.resp->begin(),
      item2.resp->end(),
      std::begin(expected),
      std::end(expected));
   BOOST_TEST(item2.done);
}

// Cancellation cases
// If the request is waiting, we just remove it
void test_cancel_waiting()
{
   // Setup
   multiplexer mpx;
   auto item1 = std::make_unique<test_item>();
   auto item2 = std::make_unique<test_item>();
   mpx.add(item1->elem_ptr);
   mpx.add(item2->elem_ptr);

   // Cancel the first request
   mpx.cancel(item1->elem_ptr);
   item1.reset();  // Verify we don't reference this item anyhow

   // We can progress the other request normally
   BOOST_TEST_EQ(mpx.prepare_write(), 1u);
   BOOST_TEST_EQ(mpx.commit_write(), 0u);
   error_code ec;
   auto res = mpx.consume_next("$11\r\nHello world\r\n", ec);
   BOOST_TEST_EQ(res.first, consume_result::got_response);
   BOOST_TEST(item2->done);
   const node expected[] = {
      {type::blob_string, 1u, 0u, "Hello world"},
   };
   BOOST_TEST_ALL_EQ(
      item2->resp->begin(),
      item2->resp->end(),
      std::begin(expected),
      std::end(expected));
}

// If the request is staged, we mark it as abandoned
void test_cancel_staged()
{
   // Setup
   multiplexer mpx;
   auto item1 = std::make_unique<test_item>();
   auto item2 = std::make_unique<test_item>();
   mpx.add(item1->elem_ptr);
   mpx.add(item2->elem_ptr);

   // A write starts
   BOOST_TEST_EQ(mpx.prepare_write(), 2u);

   // Cancel the first request
   mpx.cancel(item1->elem_ptr);
   item1.reset();  // Verify we don't reference this item anyhow

   // The write gets confirmed
   BOOST_TEST_EQ(mpx.commit_write(), 0u);

   // The cancelled request's response arrives. It gets discarded
   error_code ec;
   auto res = mpx.consume_next("+Goodbye\r\n", ec);
   BOOST_TEST_EQ(res.first, consume_result::got_response);
   BOOST_TEST_NOT(item2->done);

   // The 2nd request's response arrives. It gets parsed successfully
   res = mpx.consume_next("$11\r\nHello world\r\n", ec);
   BOOST_TEST_EQ(res.first, consume_result::got_response);
   BOOST_TEST(item2->done);
   const node expected[] = {
      {type::blob_string, 1u, 0u, "Hello world"},
   };
   BOOST_TEST_ALL_EQ(
      item2->resp->begin(),
      item2->resp->end(),
      std::begin(expected),
      std::end(expected));
}

//   cancel staged, no response expected
//   cancel written
//   cancel written, some commands responded, some not
//   cancel written, a command is half-parsed
//   cancel_on_connection_lost cleans up cancelled requests

}  // namespace

int main()
{
   test_request_needs_more();
   test_several_requests();
   test_request_response_before_write();
   test_push();
   test_push_needs_more();
   test_push_heuristics_no_request();
   test_push_heuristics_request_without_response();
   test_push_heuristics_request_waiting();
   test_mix_responses_pushes();
   test_cancel_on_connection_lost();
   // test_cancel_on_connection_lost_half_parsed_response();
   test_reset();

   test_cancel_waiting();
   test_cancel_staged();

   return boost::report_errors();
}
