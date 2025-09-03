/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/adapter/adapt.hpp>
#include <boost/redis/adapter/any_adapter.hpp>
#include <boost/redis/detail/multiplexer.hpp>
#include <boost/redis/detail/read_buffer.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/resp3/node.hpp>
#include <boost/redis/resp3/serialization.hpp>
#include <boost/redis/resp3/type.hpp>
#include <boost/redis/response.hpp>

#define BOOST_TEST_MODULE low_level_sync_sans_io
#include <boost/test/included/unit_test.hpp>

#include <iostream>
#include <string>

using boost::redis::request;
using boost::redis::adapter::adapt2;
using boost::redis::adapter::result;
using boost::redis::detail::multiplexer;
using boost::redis::generic_response;
using boost::redis::ignore_t;
using boost::redis::resp3::detail::deserialize;
using boost::redis::resp3::node;
using boost::redis::resp3::to_string;
using boost::redis::response;
using boost::redis::any_adapter;
using boost::system::error_code;

#define RESP3_SET_PART1 "~6\r\n+orange\r"
#define RESP3_SET_PART2 "\n+apple\r\n+one"
#define RESP3_SET_PART3 "\r\n+two\r"
#define RESP3_SET_PART4 "\n+three\r\n+orange\r\n"
char const* resp3_set = RESP3_SET_PART1 RESP3_SET_PART2 RESP3_SET_PART3 RESP3_SET_PART4;

BOOST_AUTO_TEST_CASE(low_level_sync_sans_io)
{
   try {
      result<std::set<std::string>> resp;

      deserialize(resp3_set, adapt2(resp));

      for (auto const& e : resp.value())
         std::cout << e << std::endl;

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
      exit(EXIT_FAILURE);
   }
}

BOOST_AUTO_TEST_CASE(issue_210_empty_set)
{
   try {
      result<std::tuple<
         result<int>,
         result<std::vector<std::string>>,
         result<std::string>,
         result<int>>>
         resp;

      char const* wire = "*4\r\n:1\r\n~0\r\n$25\r\nthis_should_not_be_in_set\r\n:2\r\n";

      deserialize(wire, adapt2(resp));

      BOOST_CHECK_EQUAL(std::get<0>(resp.value()).value(), 1);
      BOOST_CHECK(std::get<1>(resp.value()).value().empty());
      BOOST_CHECK_EQUAL(std::get<2>(resp.value()).value(), "this_should_not_be_in_set");
      BOOST_CHECK_EQUAL(std::get<3>(resp.value()).value(), 2);

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
      exit(EXIT_FAILURE);
   }
}

BOOST_AUTO_TEST_CASE(issue_210_non_empty_set_size_one)
{
   try {
      result<std::tuple<
         result<int>,
         result<std::vector<std::string>>,
         result<std::string>,
         result<int>>>
         resp;

      char const*
         wire = "*4\r\n:1\r\n~1\r\n$3\r\nfoo\r\n$25\r\nthis_should_not_be_in_set\r\n:2\r\n";

      deserialize(wire, adapt2(resp));

      BOOST_CHECK_EQUAL(std::get<0>(resp.value()).value(), 1);
      BOOST_CHECK_EQUAL(std::get<1>(resp.value()).value().size(), 1u);
      BOOST_CHECK_EQUAL(std::get<1>(resp.value()).value().at(0), std::string{"foo"});
      BOOST_CHECK_EQUAL(std::get<2>(resp.value()).value(), "this_should_not_be_in_set");
      BOOST_CHECK_EQUAL(std::get<3>(resp.value()).value(), 2);

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
      exit(EXIT_FAILURE);
   }
}

BOOST_AUTO_TEST_CASE(issue_210_non_empty_set_size_two)
{
   try {
      result<std::tuple<
         result<int>,
         result<std::vector<std::string>>,
         result<std::string>,
         result<int>>>
         resp;

      char const* wire =
         "*4\r\n:1\r\n~2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n$25\r\nthis_should_not_be_in_set\r\n:2\r\n";

      deserialize(wire, adapt2(resp));

      BOOST_CHECK_EQUAL(std::get<0>(resp.value()).value(), 1);
      BOOST_CHECK_EQUAL(std::get<1>(resp.value()).value().at(0), std::string{"foo"});
      BOOST_CHECK_EQUAL(std::get<1>(resp.value()).value().at(1), std::string{"bar"});
      BOOST_CHECK_EQUAL(std::get<2>(resp.value()).value(), "this_should_not_be_in_set");

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
      exit(EXIT_FAILURE);
   }
}

BOOST_AUTO_TEST_CASE(issue_210_no_nested)
{
   try {
      result<std::tuple<result<int>, result<std::string>, result<std::string>, result<std::string>>>
         resp;

      char const*
         wire = "*4\r\n:1\r\n$3\r\nfoo\r\n$3\r\nbar\r\n$25\r\nthis_should_not_be_in_set\r\n";

      deserialize(wire, adapt2(resp));

      BOOST_CHECK_EQUAL(std::get<0>(resp.value()).value(), 1);
      BOOST_CHECK_EQUAL(std::get<1>(resp.value()).value(), std::string{"foo"});
      BOOST_CHECK_EQUAL(std::get<2>(resp.value()).value(), std::string{"bar"});
      BOOST_CHECK_EQUAL(std::get<3>(resp.value()).value(), "this_should_not_be_in_set");

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
      exit(EXIT_FAILURE);
   }
}

BOOST_AUTO_TEST_CASE(issue_233_array_with_null)
{
   try {
      result<std::vector<std::optional<std::string>>> resp;

      char const* wire = "*3\r\n+one\r\n_\r\n+two\r\n";
      deserialize(wire, adapt2(resp));

      BOOST_CHECK_EQUAL(resp.value().at(0).value(), "one");
      BOOST_TEST(!resp.value().at(1).has_value());
      BOOST_CHECK_EQUAL(resp.value().at(2).value(), "two");

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
      exit(EXIT_FAILURE);
   }
}

BOOST_AUTO_TEST_CASE(issue_233_optional_array_with_null)
{
   try {
      result<std::optional<std::vector<std::optional<std::string>>>> resp;

      char const* wire = "*3\r\n+one\r\n_\r\n+two\r\n";
      deserialize(wire, adapt2(resp));

      BOOST_CHECK_EQUAL(resp.value().value().at(0).value(), "one");
      BOOST_TEST(!resp.value().value().at(1).has_value());
      BOOST_CHECK_EQUAL(resp.value().value().at(2).value(), "two");

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
      exit(EXIT_FAILURE);
   }
}

//===========================================================================
// Multiplexer

std::ostream& operator<<(std::ostream& os, node const& nd)
{
   os << to_string(nd.data_type) << "\n"
      << nd.aggregate_size << "\n"
      << nd.depth << "\n"
      << nd.value;

   return os;
}

BOOST_AUTO_TEST_CASE(multiplexer_push)
{
   multiplexer mpx;
   generic_response resp;
   mpx.set_receive_adapter(any_adapter{resp});

   boost::system::error_code ec;
   auto const ret = mpx.consume_next(">2\r\n+one\r\n+two\r\n", ec);

   BOOST_TEST(ret.first.value());
   BOOST_CHECK_EQUAL(ret.second, 16u);

   // TODO: Provide operator << for generic_response so we can compare
   // the whole vector.
   BOOST_CHECK_EQUAL(resp.value().size(), 3u);
   BOOST_CHECK_EQUAL(resp.value().at(1).value, "one");
   BOOST_CHECK_EQUAL(resp.value().at(2).value, "two");

   for (auto const& e : resp.value())
      std::cout << e << std::endl;
}

BOOST_AUTO_TEST_CASE(multiplexer_push_needs_more)
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
   BOOST_CHECK_EQUAL(ret.second, 16u);

   // TODO: Provide operator << for generic_response so we can compare
   // the whole vector.
   BOOST_CHECK_EQUAL(resp.value().size(), 3u);
   BOOST_CHECK_EQUAL(resp.value().at(1).value, "one");
   BOOST_CHECK_EQUAL(resp.value().at(2).value, "two");
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

BOOST_AUTO_TEST_CASE(multiplexer_pipeline)
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
   BOOST_CHECK_EQUAL(mpx.prepare_write(), 3u);
   BOOST_CHECK_EQUAL(mpx.prepare_write(), 0u);

   // After coalescing the requests for writing their statuses should
   // be changed to "staged".
   BOOST_TEST(item1.elem_ptr->is_staged());
   BOOST_TEST(item2.elem_ptr->is_staged());
   BOOST_TEST(item3.elem_ptr->is_staged());

   // There are no waiting requests to cancel since they are all
   // staged.
   BOOST_CHECK_EQUAL(mpx.cancel_waiting(), 0u);

   // Since the requests haven't been sent (written) the done
   // callback should not have been called yet.
   BOOST_TEST(!item1.done);
   BOOST_TEST(!item2.done);
   BOOST_TEST(!item3.done);

   // The commit_write call informs the multiplexer the payload was
   // sent (e.g.  written to the socket). This step releases requests
   // that has no response.
   BOOST_CHECK_EQUAL(mpx.commit_write(), 1u);

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

BOOST_AUTO_TEST_CASE(read_buffer_prepare_error)
{
   using boost::redis::detail::read_buffer;

   read_buffer buf;

   // Usual case, max size is bigger then requested size.
   buf.set_config({10, 10});
   auto ec = buf.prepare_append();
   BOOST_TEST(!ec);
   buf.commit_append(10);

   // Corner case, max size is equal to the requested size.
   buf.set_config({10, 20});
   ec = buf.prepare_append();
   BOOST_TEST(!ec);
   buf.commit_append(10);
   buf.consume_committed(20);

   auto const tmp = buf;

   // Error case, max size is smaller to the requested size.
   buf.set_config({10, 9});
   ec = buf.prepare_append();
   BOOST_TEST(ec == error_code{boost::redis::error::exceeds_maximum_read_buffer_size});

   // Check that an error call has no side effects.
   auto const res = buf == tmp;
   BOOST_TEST(res);
}

BOOST_AUTO_TEST_CASE(read_buffer_prepare_consume_only_committed_data)
{
   using boost::redis::detail::read_buffer;

   read_buffer buf;

   buf.set_config({10, 10});
   auto ec = buf.prepare_append();
   BOOST_TEST(!ec);

   // No data has been committed yet so nothing can be consummed.
   BOOST_CHECK_EQUAL(buf.consume_committed(5), 0u);

   buf.commit_append(10);

   // All five bytes can be consumed.
   BOOST_CHECK_EQUAL(buf.consume_committed(5), 5u);

   // Only the remaining five bytes can be consumed
   BOOST_CHECK_EQUAL(buf.consume_committed(7), 5u);
}

BOOST_AUTO_TEST_CASE(read_buffer_check_buffer_size)
{
   using boost::redis::detail::read_buffer;

   read_buffer buf;

   buf.set_config({10, 10});
   auto ec = buf.prepare_append();
   BOOST_TEST(!ec);

   BOOST_CHECK_EQUAL(buf.get_append_buffer().size(), 10u);
}

BOOST_AUTO_TEST_CASE(check_counter_adapter)
{
   using boost::redis::any_adapter;
   using boost::redis::resp3::parse;
   using boost::redis::resp3::parser;
   using boost::redis::resp3::node_view;
   using boost::system::error_code;

   int init = 0;
   int node = 0;
   int done = 0;

   auto counter_adapter = [&](any_adapter::parse_event ev, node_view const&, error_code&) mutable {
      switch (ev) {
         case any_adapter::parse_event::init: init++; break;
         case any_adapter::parse_event::node: node++; break;
         case any_adapter::parse_event::done: done++; break;
      }
   };

   any_adapter wrapped{any_adapter::impl_t{counter_adapter}};

   error_code ec;
   parser p;

   auto const ret1 = parse(p, RESP3_SET_PART1, wrapped, ec);
   auto const ret2 = parse(p, RESP3_SET_PART1 RESP3_SET_PART2, wrapped, ec);
   auto const ret3 = parse(p, RESP3_SET_PART1 RESP3_SET_PART2 RESP3_SET_PART3, wrapped, ec);
   auto const ret4 = parse(
      p,
      RESP3_SET_PART1 RESP3_SET_PART2 RESP3_SET_PART3 RESP3_SET_PART4,
      wrapped,
      ec);

   BOOST_TEST(!ret1);
   BOOST_TEST(!ret2);
   BOOST_TEST(!ret3);
   BOOST_TEST(ret4);

   BOOST_CHECK_EQUAL(init, 1);
   BOOST_CHECK_EQUAL(node, 7);
   BOOST_CHECK_EQUAL(done, 1);
}
