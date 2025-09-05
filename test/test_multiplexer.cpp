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
#include <string>

using boost::redis::request;
using boost::redis::detail::multiplexer;
using boost::redis::generic_response;
using boost::redis::resp3::node;
using boost::redis::resp3::to_string;
using boost::redis::response;
using boost::redis::any_adapter;
using boost::system::error_code;

namespace boost::redis::resp3 {

std::ostream& operator<<(std::ostream& os, node const& nd)
{
   os << to_string(nd.data_type) << "\n"
      << nd.aggregate_size << "\n"
      << nd.depth << "\n"
      << nd.value;

   return os;
}

}  // namespace boost::redis::resp3

namespace {

void test_push()
{
   multiplexer mpx;
   generic_response resp;
   mpx.set_receive_adapter(any_adapter{resp});

   boost::system::error_code ec;
   auto const ret = mpx.consume_next(">2\r\n+one\r\n+two\r\n", ec);

   BOOST_TEST(ret.first.value());
   BOOST_TEST_EQ(ret.second, 16u);

   // TODO: Provide operator << for generic_response so we can compare
   // the whole vector.
   BOOST_TEST_EQ(resp.value().size(), 3u);
   BOOST_TEST_EQ(resp.value().at(1).value, "one");
   BOOST_TEST_EQ(resp.value().at(2).value, "two");

   for (auto const& e : resp.value())
      std::cout << e << std::endl;
}

void test_push_needs_more()
{
   multiplexer mpx;
   generic_response resp;
   mpx.set_receive_adapter(any_adapter{resp});

   std::string msg;
   // Only part of the message.
   msg += ">2\r\n+one\r";

   boost::system::error_code ec;
   auto ret = mpx.consume_next(msg, ec);

   BOOST_TEST(!ret.first.has_value());

   msg += "\n+two\r\n";
   ret = mpx.consume_next(msg, ec);

   BOOST_TEST(ret.first.value());
   BOOST_TEST_EQ(ret.second, 16u);

   // TODO: Provide operator << for generic_response so we can compare
   // the whole vector.
   BOOST_TEST_EQ(resp.value().size(), 3u);
   BOOST_TEST_EQ(resp.value().at(1).value, "one");
   BOOST_TEST_EQ(resp.value().at(2).value, "two");
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

void test_pipeline()
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
   boost::system::error_code ec;
   auto const ret = mpx.consume_next("+one\r\n", ec);

   // The read operation should have been successfull.
   BOOST_TEST(ret.first.has_value());
   BOOST_TEST(ret.second != 0u);

   // The last request still did not get a response.
   BOOST_TEST(item1.done);
   BOOST_TEST(item2.done);
   BOOST_TEST(!item3.done);

   // TODO: Check the first request was removed from the queue.
}

}  // namespace

int main()
{
   test_push();
   test_push_needs_more();
   test_pipeline();

   return boost::report_errors();
}
