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

// We correctly reset parsing state between requests and pushes
void test_mix_responses_pushes()
{
   // Setup
   multiplexer mpx;
   generic_response push_resp;
   mpx.set_receive_adapter(any_adapter{push_resp});
   test_item item1, item2{};

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
   error_code ec;
   auto ret = mpx.consume_next(">2\r\n+one\r\n+two\r\n", ec);
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
   ret = mpx.consume_next("$11\r\nHello world\r\n", ec);
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
   ret = mpx.consume_next(">2\r\n+other\r\n+push\r\n", ec);
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
   ret = mpx.consume_next("$8\r\nResponse\r\n", ec);
   BOOST_TEST_EQ(ret.first, consume_result::got_response);
   BOOST_TEST_EQ(ret.second, 14u);
   BOOST_TEST(item2.resp.has_value());
   expected = {
      {type::blob_string, 1u, 0u, "Response"},
   };
   BOOST_TEST_ALL_EQ(item2.resp->begin(), item2.resp->end(), expected.begin(), expected.end());
   BOOST_TEST(item1.done);
   BOOST_TEST(item2.done);
}

}  // namespace

int main()
{
   test_push();
   test_push_needs_more();
   test_request_needs_more();
   test_several_requests();
   test_mix_responses_pushes();

   return boost::report_errors();
}
