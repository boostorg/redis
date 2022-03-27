/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <map>
#include <iostream>
#include <optional>

#include <boost/system/errc.hpp>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

#include "test_stream.hpp"
#include "check.hpp"

// Consider using Beast test_stream and instantiate the test socket
// only once.

namespace net = aedis::net;
namespace resp3 = aedis::resp3;
using test_tcp_socket = net::use_awaitable_t<>::as_default_on_t<aedis::test_stream<aedis::net::system_executor>>;
using aedis::adapter::adapt;
using node_type = aedis::adapter::node<std::string>;

//-------------------------------------------------------------------

template <class Result>
struct expect {
   std::string in;
   Result expected;
   std::string name;
   boost::system::error_condition ec;
};

template <class Result>
void test_sync(expect<Result> e)
{
   std::string rbuffer;
   test_tcp_socket ts {e.in};
   Result result;
   boost::system::error_code ec;
   resp3::read(ts, net::dynamic_buffer(rbuffer), adapt(result), ec);
   expect_error(ec, e.ec);
   if (e.ec)
      return;
   check_empty(rbuffer);
   expect_eq(result, e.expected, e.name);
}

template <class Result>
net::awaitable<void>
test_async(expect<Result> e)
{
   std::string rbuffer;
   test_tcp_socket ts {e.in};
   Result result;
   boost::system::error_code ec;
   co_await resp3::async_read(ts, net::dynamic_buffer(rbuffer), adapt(result), net::redirect_error(net::use_awaitable, ec));
   expect_error(ec, e.ec);
   if (e.ec)
      co_return;
   check_empty(rbuffer);
   expect_eq(result, e.expected, e.name);
}

void test_number(net::io_context& ioc)
{
   std::optional<int> ok;
   ok = 11;

   // Success
   auto const in13 = expect<node_type>{":3\r\n", node_type{resp3::type::number, 1UL, 0UL, {"3"}}, "number.node (positive)"};
   auto const in12 = expect<node_type>{":-3\r\n", node_type{resp3::type::number, 1UL, 0UL, {"-3"}}, "number.node (negative)"};
   auto const in15 = expect<int>{":11\r\n", int{11}, "number.int"};
   auto const in16 = expect<std::optional<int>>{":11\r\n", ok, "adapter.number.optional.int"};

   // Transaction.
   auto const in17 = expect<std::tuple<int>>{"*1\r\n:11\r\n", std::tuple<int>{11}, "adapter.number.tuple.int"};

   // Error
   auto const in02 = expect<int>{":adf\r\n", int{11}, "adapter.number.int", boost::system::errc::errc_t::invalid_argument};
   auto const in11 = expect<std::optional<int>>{":adf\r\n", std::optional<int>{}, "adapter.number.optional.int", boost::system::errc::errc_t::invalid_argument};
   auto const in03 = expect<int>{":\r\n", int{}, "adapter.number.error (empty field)", aedis::resp3::make_error_condition(aedis::resp3::error::empty_field)};
   auto const in04 = expect<std::optional<int>>{"%11\r\n", std::optional<int>{}, "adapter.number.optional.int", aedis::adapter::make_error_condition(aedis::adapter::error::expects_simple_type)};
   auto const in05 = expect<std::vector<std::string>>{":11\r\n", std::vector<std::string>{}, "adapter.number.optional.int", aedis::adapter::make_error_condition(aedis::adapter::error::expects_aggregate)};
   auto const in06 = expect<std::set<std::string>>{":11\r\n", std::set<std::string>{}, "adapter.number.optional.int", aedis::adapter::make_error_condition(aedis::adapter::error::expects_aggregate)};
   auto const in07 = expect<std::unordered_set<std::string>>{":11\r\n", std::unordered_set<std::string>{}, "adapter.number.optional.int", aedis::adapter::make_error_condition(aedis::adapter::error::expects_aggregate)};
   auto const in08 = expect<std::map<std::string, std::string>>{":11\r\n", std::map<std::string, std::string>{}, "adapter.number.optional.int", aedis::adapter::make_error_condition(aedis::adapter::error::expects_aggregate)};
   auto const in09 = expect<std::unordered_map<std::string, std::string>>{":11\r\n", std::unordered_map<std::string, std::string>{}, "adapter.number.optional.int", aedis::adapter::make_error_condition(aedis::adapter::error::expects_aggregate)};
   auto const in10 = expect<std::list<std::string>>{":11\r\n", std::list<std::string>{}, "adapter.number.optional.int", aedis::adapter::make_error_condition(aedis::adapter::error::expects_aggregate)};

   test_sync(in02);
   test_sync(in03);
   test_sync(in04);
   test_sync(in05);
   test_sync(in06);
   test_sync(in07);
   test_sync(in08);
   test_sync(in09);
   test_sync(in10);
   test_sync(in11);
   test_sync(in12);
   test_sync(in13);
   test_sync(in15);
   test_sync(in16);
   test_sync(in17);

   co_spawn(ioc, test_async(in02), net::detached);
   co_spawn(ioc, test_async(in03), net::detached);
   co_spawn(ioc, test_async(in04), net::detached);
   co_spawn(ioc, test_async(in05), net::detached);
   co_spawn(ioc, test_async(in06), net::detached);
   co_spawn(ioc, test_async(in07), net::detached);
   co_spawn(ioc, test_async(in08), net::detached);
   co_spawn(ioc, test_async(in09), net::detached);
   co_spawn(ioc, test_async(in10), net::detached);
   co_spawn(ioc, test_async(in11), net::detached);
   co_spawn(ioc, test_async(in12), net::detached);
   co_spawn(ioc, test_async(in13), net::detached);
   co_spawn(ioc, test_async(in15), net::detached);
   co_spawn(ioc, test_async(in16), net::detached);
   co_spawn(ioc, test_async(in17), net::detached);
}

void test_bool(net::io_context& ioc)
{
   std::optional<bool> ok;
   ok = true;

   // Success.
   auto const in8 = expect<node_type>{"#f\r\n", node_type{resp3::type::boolean, 1UL, 0UL, {"f"}}, "bool.node (false)"};
   auto const in9 = expect<node_type>{"#t\r\n", node_type{resp3::type::boolean, 1UL, 0UL, {"t"}}, "bool.node (true)"};
   auto const in10 = expect<bool>{"#t\r\n", bool{true}, "bool.bool (true)"};
   auto const in11 = expect<bool>{"#f\r\n", bool{false}, "bool.bool (true)"};
   auto const in13 = expect<std::optional<bool>>{"#t\r\n", ok, "adapter.optional.int"};

   // Error
   auto const in1 = expect<std::optional<bool>>{"#11\r\n", std::optional<bool>{}, "adapter.bool.error", aedis::resp3::make_error_condition(aedis::resp3::error::unexpected_bool_value)};
   auto const in2 = expect<std::optional<bool>>{"#\r\n", std::optional<bool>{}, "adapter.bool.error", aedis::resp3::make_error_condition(aedis::resp3::error::empty_field)};
   auto const in3 = expect<std::set<int>>{"#t\r\n", std::set<int>{}, "adapter.bool.error", aedis::adapter::make_error_condition(aedis::adapter::error::expects_aggregate)};
   auto const in4 = expect<std::unordered_set<int>>{"#t\r\n", std::unordered_set<int>{}, "adapter.bool.error", aedis::adapter::make_error_condition(aedis::adapter::error::expects_aggregate)};
   auto const in5 = expect<std::map<int, int>>{"#t\r\n", std::map<int, int>{}, "adapter.bool.error", aedis::adapter::make_error_condition(aedis::adapter::error::expects_aggregate)};
   auto const in6 = expect<std::unordered_map<int, int>>{"#t\r\n", std::unordered_map<int, int>{}, "adapter.bool.error", aedis::adapter::make_error_condition(aedis::adapter::error::expects_aggregate)};
   auto const in7 = expect<std::vector<int>>{"#t\r\n", std::vector<int>{}, "adapter.bool.error", aedis::adapter::make_error_condition(aedis::adapter::error::expects_aggregate)};

   test_sync(in1);
   test_sync(in2);
   test_sync(in3);
   test_sync(in4);
   test_sync(in5);
   test_sync(in6);
   test_sync(in7);
   test_sync(in8);
   test_sync(in9);
   test_sync(in10);
   test_sync(in11);

   co_spawn(ioc, test_async(in1), net::detached);
   co_spawn(ioc, test_async(in2), net::detached);
   co_spawn(ioc, test_async(in3), net::detached);
   co_spawn(ioc, test_async(in4), net::detached);
   co_spawn(ioc, test_async(in5), net::detached);
   co_spawn(ioc, test_async(in6), net::detached);
   co_spawn(ioc, test_async(in7), net::detached);
   co_spawn(ioc, test_async(in8), net::detached);
   co_spawn(ioc, test_async(in9), net::detached);
   co_spawn(ioc, test_async(in10), net::detached);
   co_spawn(ioc, test_async(in11), net::detached);
}

void test_streamed_string(net::io_context& ioc)
{
   std::string const wire = "$?\r\n;4\r\nHell\r\n;5\r\no wor\r\n;1\r\nd\r\n;0\r\n";

   std::vector<node_type> e1a
   { {aedis::resp3::type::streamed_string_part, 1, 1, "Hell"}
   , {aedis::resp3::type::streamed_string_part, 1, 1, "o wor"}
   , {aedis::resp3::type::streamed_string_part, 1, 1, "d"}
   };

   // TODO: The parser is producing the wrong answer.
   //std::vector<node_type> e1b { {resp3::type::streamed_string_part, 0UL, 0UL, {}} };
   std::vector<node_type> e1b {};

   auto const in01 = expect<std::vector<node_type>>{wire, e1a, "streamed_string.node"};
   auto const in03 = expect<std::string>{wire, std::string{"Hello word"}, "streamed_string.string"};
   auto const in02 = expect<std::vector<node_type>>{"$?\r\n;0\r\n", e1b, "streamed_string.node.empty"};

   test_sync(in01);
   test_sync(in02);
   test_sync(in03);

   co_spawn(ioc, test_async(in01), net::detached);
   co_spawn(ioc, test_async(in02), net::detached);
   co_spawn(ioc, test_async(in03), net::detached);
}

void test_push(net::io_context& ioc)
{
   std::string const wire = ">4\r\n+pubsub\r\n+message\r\n+some-channel\r\n+some message\r\n";

   std::vector<node_type> e1a
      { {resp3::type::push,          4UL, 0UL, {}}
      , {resp3::type::simple_string, 1UL, 1UL, "pubsub"}
      , {resp3::type::simple_string, 1UL, 1UL, "message"}
      , {resp3::type::simple_string, 1UL, 1UL, "some-channel"}
      , {resp3::type::simple_string, 1UL, 1UL, "some message"}
      };

   std::vector<node_type> e1b { {resp3::type::push, 0UL, 0UL, {}} };

   auto const in01 = expect<std::vector<node_type>>{wire, e1a, "push.node"};
   auto const in02 = expect<std::vector<node_type>>{">0\r\n", e1b, "push.node.empty"};

   test_sync(in01);
   test_sync(in02);

   co_spawn(ioc, test_async(in01), net::detached);
   co_spawn(ioc, test_async(in02), net::detached);
}

void test_map(net::io_context& ioc)
{
   using map_type = std::map<std::string, std::string>;
   using vec_type = std::vector<std::string>;
   using op_map_type = std::optional<std::map<std::string, std::string>>;
   using op_vec_type = std::optional<std::vector<std::string>>;

   std::string const wire = "%3\r\n$4\r\nkey1\r\n$6\r\nvalue1\r\n$4\r\nkey2\r\n$6\r\nvalue2\r\n$4\r\nkey3\r\n$6\r\nvalue3\r\n";

   std::vector<node_type> expected_1a
   { {resp3::type::map,         3UL, 0UL, {}}
   , {resp3::type::blob_string, 1UL, 1UL, {"key1"}}
   , {resp3::type::blob_string, 1UL, 1UL, {"value1"}}
   , {resp3::type::blob_string, 1UL, 1UL, {"key2"}}
   , {resp3::type::blob_string, 1UL, 1UL, {"value2"}}
   , {resp3::type::blob_string, 1UL, 1UL, {"key3"}}
   , {resp3::type::blob_string, 1UL, 1UL, {"value3"}}
   };

   map_type expected_1b
   { {"key1", "value1"}
   , {"key2", "value2"}
   , {"key3", "value3"}
   };

   std::vector<std::string> expected_1c
   { "key1", "value1"
   , "key2", "value2"
   , "key3", "value3"
   };

   op_map_type expected_1d;
   expected_1d = expected_1b;

   op_vec_type expected_1e;
   expected_1e = expected_1c;

   auto const in01 = expect<std::vector<node_type>>{wire, expected_1a, "map.node"};
   auto const in02 = expect<map_type>{"%0\r\n", map_type{}, "map.map.empty"};
   auto const in03 = expect<map_type>{wire, expected_1b, "map.map"};
   auto const in04 = expect<vec_type>{wire, expected_1c, "map.vector"};
   auto const in05 = expect<op_map_type>{wire, expected_1d, "map.optional.map"};
   auto const in07 = expect<op_vec_type>{wire, expected_1e, "map.optional.vector"};
   auto const in08 = expect<std::tuple<op_map_type>>{"*1\r\n" + wire, std::tuple<op_map_type>{expected_1d}, "map.transaction.optional.map"};
   auto const in09 = expect<int>{"%11\r\n", int{}, "map.invalid.int", aedis::adapter::make_error_condition(aedis::adapter::error::expects_simple_type)};
   // TODO: Test errors

   test_sync(in01);
   test_sync(in02);
   test_sync(in03);
   test_sync(in04);
   test_sync(in05);
   test_sync(in07);
   test_sync(in08);
   test_sync(in09);

   co_spawn(ioc, test_async(in01), net::detached);
   co_spawn(ioc, test_async(in02), net::detached);
   co_spawn(ioc, test_async(in03), net::detached);
   co_spawn(ioc, test_async(in04), net::detached);
   co_spawn(ioc, test_async(in05), net::detached);
   co_spawn(ioc, test_async(in07), net::detached);
   co_spawn(ioc, test_async(in08), net::detached);
   co_spawn(ioc, test_async(in09), net::detached);
}

void test_set(net::io_context& ioc)
{
   using set_type = std::set<std::string>;
   using vec_type = std::vector<std::string>;
   using op_vec_type = std::optional<std::vector<std::string>>;
   using uset_type = std::unordered_set<std::string>;

   std::string const wire = "~5\r\n+orange\r\n+apple\r\n+one\r\n+two\r\n+three\r\n";
   std::vector<node_type> const expected1a
   { {resp3::type::set,            5UL, 0UL, {}}
   , {resp3::type::simple_string,  1UL, 1UL, {"orange"}}
   , {resp3::type::simple_string,  1UL, 1UL, {"apple"}}
   , {resp3::type::simple_string,  1UL, 1UL, {"one"}}
   , {resp3::type::simple_string,  1UL, 1UL, {"two"}}
   , {resp3::type::simple_string,  1UL, 1UL, {"three"}}
   };

   set_type const e1b{"apple", "one", "orange", "three", "two"};
   uset_type const e1c{"apple", "one", "orange", "three", "two"};
   vec_type const e1d = {"orange", "apple", "one", "two", "three"};
   op_vec_type expected_1e;
   expected_1e = e1d;

   auto const in01 = expect<std::vector<node_type>>{wire, expected1a, "set.node"};
   auto const in02 = expect<std::vector<node_type>>{"~0\r\n", std::vector<node_type>{ {resp3::type::set,  0UL, 0UL, {}} }, "set.node (empty)"};
   auto const in03 = expect<set_type>{wire, e1b, "set.set (empty)"};
   auto const in04 = expect<vec_type>{wire, e1d, "set.vector "};
   auto const in05 = expect<op_vec_type>{wire, expected_1e, "set.vector "};
   auto const in06 = expect<uset_type>{wire, e1c, "set.unordered_set"};
   auto const in07 = expect<std::tuple<uset_type>>{"*1\r\n" + wire, std::tuple<uset_type>{e1c}, "set.unordered_set"};

   test_sync(in01);
   test_sync(in02);
   test_sync(in03);
   test_sync(in04);
   test_sync(in05);
   test_sync(in06);
   test_sync(in07);

   co_spawn(ioc, test_async(in01), net::detached);
   co_spawn(ioc, test_async(in02), net::detached);
   co_spawn(ioc, test_async(in03), net::detached);
   co_spawn(ioc, test_async(in04), net::detached);
   co_spawn(ioc, test_async(in05), net::detached);
   co_spawn(ioc, test_async(in06), net::detached);
   co_spawn(ioc, test_async(in07), net::detached);
}

// TODO: Test a large simple string. For example
//   std::string str(10000, 'a');
//   std::string cmd;
//   cmd += '+';
//   cmd += str;
//   cmd += "\r\n";

//-------------------------------------------------------------------------

net::awaitable<void> test_async_array()
{
   test_tcp_socket ts {"*3\r\n$2\r\n11\r\n$2\r\n22\r\n$1\r\n3\r\n"};
   std::string buf;
   auto dbuf = net::dynamic_buffer(buf);

   {
      std::vector<node_type> result;
      boost::system::error_code ec;
      co_await resp3::async_read(ts, dbuf, adapt(result), net::redirect_error(net::use_awaitable, ec));

      std::vector<node_type> expected
	 { {resp3::type::array,       3UL, 0UL, {}}
	 , {resp3::type::blob_string, 1UL, 1UL, {"11"}}
	 , {resp3::type::blob_string, 1UL, 1UL, {"22"}}
	 , {resp3::type::blob_string, 1UL, 1UL, {"3"}}
         };

      expect_eq(result, expected, "array (node-async)");
   }

   {
      std::vector<int> result;
      boost::system::error_code ec;
      co_await resp3::async_read(ts, dbuf, adapt(result), net::redirect_error(net::use_awaitable, ec));
      expect_error(ec);

      std::vector<int> expected {11, 22, 3};
      expect_eq(result, expected, "array (int-async)");
   }

   {
      test_tcp_socket ts {"*0\r\n"};

      std::vector<int> result;
      boost::system::error_code ec;
      co_await resp3::async_read(ts, dbuf, adapt(result), net::redirect_error(net::use_awaitable, ec));
      expect_error(ec);

      std::vector<int> expected;
      expect_eq(result, expected, "array (empty)");
   }
}

net::awaitable<void> test_async_attribute()
{
   {
      test_tcp_socket ts{"|1\r\n+key-popularity\r\n%2\r\n$1\r\na\r\n,0.1923\r\n$1\r\nb\r\n,0.0012\r\n"};
      std::string rbuffer;
      auto dbuf = net::dynamic_buffer(rbuffer);

      std::vector<node_type> result;
      boost::system::error_code ec;
      co_await resp3::async_read(ts, dbuf, adapt(result), net::redirect_error(net::use_awaitable, ec));
      expect_error(ec);

      std::vector<node_type> expected
         { {resp3::type::attribute,     1UL, 0UL, {}}
         , {resp3::type::simple_string, 1UL, 1UL, "key-popularity"}
         , {resp3::type::map,           2UL, 1UL, {}}
         , {resp3::type::blob_string,   1UL, 2UL, "a"}
         , {resp3::type::doublean,      1UL, 2UL, "0.1923"}
         , {resp3::type::blob_string,   1UL, 2UL, "b"}
         , {resp3::type::doublean,      1UL, 2UL, "0.0012"}
         };

      expect_eq(result, expected, "attribute.async");
   }

   {
      test_tcp_socket ts{"|0\r\n"};
      std::string rbuffer;
      auto dbuf = net::dynamic_buffer(rbuffer);

      std::vector<node_type> result;
      boost::system::error_code ec;
      co_await resp3::async_read(ts, dbuf, adapt(result), net::redirect_error(net::use_awaitable, ec));
      expect_error(ec);

      std::vector<node_type> expected{{resp3::type::attribute, 0UL, 0UL, {}}};
      expect_eq(result, expected, "attribute.async.empty");
   }
}

void test_simple(net::io_context& ioc)
{
   std::vector<expect<node_type>> const simple
   // Simple string
   { {"+OK\r\n", node_type{resp3::type::simple_string, 1UL, 0UL, {"OK"}}, "simple_string.node"}
   , {"+\r\n", node_type{resp3::type::simple_string, 1UL, 0UL, {""}}, "simple_string.node.empty"}
   // Simple error
   , {"-Error\r\n", node_type{resp3::type::simple_error, 1UL, 0UL, {"Error"}}, "simple_error.node"}
   , {"-\r\n", node_type{resp3::type::simple_error, 1UL, 0UL, {""}}, "simple_error.node.empty"}
   // Blob string. TODO: test with a long that require many calls to async_read_some (fix test_stream first).
   , {"$2\r\nhh\r\n", node_type{resp3::type::blob_string, 1UL, 0UL, {"hh"}}, "blob_string.node"}
   , {"$26\r\nhhaa\aaaa\raaaaa\r\naaaaaaaaaa\r\n", node_type{resp3::type::blob_string, 1UL, 0UL, {"hhaa\aaaa\raaaaa\r\naaaaaaaaaa"}}, "blob_string.node (with separator)"}
   , {"$0\r\n\r\n", node_type{resp3::type::blob_string, 1UL, 0UL, {}}, "blob_string.node.empty"}
   // Double
   , {",1.23\r\n", node_type{resp3::type::doublean, 1UL, 0UL, {"1.23"}}, "double.node"}
   , {",inf\r\n", node_type{resp3::type::doublean, 1UL, 0UL, {"inf"}}, "double.node (inf)"}
   , {",-inf\r\n", node_type{resp3::type::doublean, 1UL, 0UL, {"-inf"}}, "double.node (-inf)"}
   // Blob error
   , {"!21\r\nSYNTAX invalid syntax\r\n", node_type{resp3::type::blob_error, 1UL, 0UL, {"SYNTAX invalid syntax"}}, "blob_error"}
   , {"!0\r\n\r\n", node_type{resp3::type::blob_error, 1UL, 0UL, {}}, "blob_error.empty"}
   // Verbatim string
   , {"=15\r\ntxt:Some string\r\n", node_type{resp3::type::verbatim_string, 1UL, 0UL, {"txt:Some string"}}, "verbatim_string"}
   , {"=0\r\n\r\n", node_type{resp3::type::verbatim_string, 1UL, 0UL, {}}, "verbatim_string.empty"}
   // Null
   , {"_\r\n", node_type{resp3::type::null, 1UL, 0UL, {}}, "null"}
   // Big number
   , {"(3492890328409238509324850943850943825024385\r\n", node_type{resp3::type::big_number, 1UL, 0UL, {"3492890328409238509324850943850943825024385"}}, "big_number"}
   };

   // Tests
   for (auto const& e: simple)
      test_sync(e);

   for (auto const& e: simple)
     co_spawn(ioc, test_async(e), net::detached);
}

void test_simple_string(net::io_context& ioc)
{
   std::optional<std::string> ok1, ok2;
   ok1 = "OK";
   ok2 = "";

   test_sync(expect<std::string>{"+OK\r\n", std::string{"OK"}, "simple_string.string"});
   test_sync(expect<std::string>{"+\r\n", std::string{""}, "simple_string.string.empty"});
   test_sync(expect<std::optional<std::string>>{"+OK\r\n", std::optional<std::string>{"OK"}, "simple_string.optional"});
   test_sync(expect<std::optional<std::string>>{"+\r\n", std::optional<std::string>{""}, "simple_string.optional.empty"});
   co_spawn(ioc, test_async(expect<std::string>{"+OK\r\n", std::string{"OK"}, "simple_string.async.string"}), net::detached);
   co_spawn(ioc, test_async(expect<std::string>{"+\r\n", std::string{""}, "simple_string.async.string.empty"}), net::detached);
   co_spawn(ioc, test_async(expect<std::optional<std::string>>{"+OK\r\n", ok1, "simple_string.async.string.optional"}), net::detached);
   co_spawn(ioc, test_async(expect<std::optional<std::string>>{"+\r\n", ok2, "simple_string.async.string.optional.empty"}), net::detached);
}

void test_resp3(net::io_context& ioc)
{
   test_sync(expect<int>{"s11\r\n", int{}, "adapter.number.error", aedis::resp3::make_error_condition(aedis::resp3::error::invalid_type)});
}

void test_big_number(net::io_context& ioc)
{
   test_sync(expect<int>{"(\r\n", int{}, "adapter.big_number.error (empty field)", aedis::resp3::make_error_condition(aedis::resp3::error::empty_field)});
}

void test_double(net::io_context& ioc)
{
   auto const in1 = expect<std::string>{",\r\n", std::string{}, "adapter.double.error (empty field)", aedis::resp3::make_error_condition(aedis::resp3::error::empty_field)};
   test_sync(in1);
   co_spawn(ioc, test_async(in1), net::detached);
}

void test_null(net::io_context& ioc)
{
   using op_type_01 = std::optional<bool>;
   using op_type_02 = std::optional<int>;
   using op_type_03 = std::optional<std::string>;
   using op_type_04 = std::optional<std::vector<std::string>>;
   using op_type_05 = std::optional<std::list<std::string>>;
   using op_type_06 = std::optional<std::map<std::string, std::string>>;
   using op_type_07 = std::optional<std::unordered_map<std::string, std::string>>;
   using op_type_08 = std::optional<std::set<std::string>>;
   using op_type_09 = std::optional<std::unordered_set<std::string>>;

   auto const in01 = expect<op_type_01>{"_\r\n", op_type_01{}, "null.optional.bool"};
   auto const in02 = expect<op_type_02>{"_\r\n", op_type_02{}, "null.optional.int"};
   auto const in03 = expect<op_type_03>{"_\r\n", op_type_03{}, "null.optional.string"};
   auto const in04 = expect<op_type_04>{"_\r\n", op_type_04{}, "null.optional.vector"};
   auto const in05 = expect<op_type_05>{"_\r\n", op_type_05{}, "null.optional.list"};
   auto const in06 = expect<op_type_06>{"_\r\n", op_type_06{}, "null.optional.map"};
   auto const in07 = expect<op_type_07>{"_\r\n", op_type_07{}, "null.optional.unordered_map"};
   auto const in08 = expect<op_type_08>{"_\r\n", op_type_08{}, "null.optional.set"};
   auto const in09 = expect<op_type_09>{"_\r\n", op_type_09{}, "null.optional.unordered_set"};

   test_sync(in01);
   test_sync(in02);
   test_sync(in03);
   test_sync(in04);
   test_sync(in05);
   test_sync(in06);
   test_sync(in07);
   test_sync(in08);
   test_sync(in09);

   co_spawn(ioc, test_async(in01), net::detached);
   co_spawn(ioc, test_async(in02), net::detached);
   co_spawn(ioc, test_async(in03), net::detached);
   co_spawn(ioc, test_async(in04), net::detached);
   co_spawn(ioc, test_async(in05), net::detached);
   co_spawn(ioc, test_async(in06), net::detached);
   co_spawn(ioc, test_async(in07), net::detached);
   co_spawn(ioc, test_async(in08), net::detached);
   co_spawn(ioc, test_async(in09), net::detached);
}

int main()
{
   net::io_context ioc {1};

   // Simple
   test_simple_string(ioc);
   test_number(ioc);
   test_double(ioc);
   test_bool(ioc);
   test_null(ioc);
   test_big_number(ioc);
   test_simple(ioc);

   // Aggregate.
   test_set(ioc);
   test_map(ioc);
   test_push(ioc);
   test_streamed_string(ioc);

   // RESP3
   test_resp3(ioc);
   co_spawn(ioc, test_async_attribute(), net::detached);
   co_spawn(ioc, test_async_array(), net::detached);

   ioc.run();
}

