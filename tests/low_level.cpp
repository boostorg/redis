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
#include <boost/asio/awaitable.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/beast/_experimental/test/stream.hpp>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

#include "check.hpp"

namespace net = boost::asio;
namespace resp3 = aedis::resp3;

using test_stream = boost::beast::test::stream;
using aedis::adapter::adapt;
using node_type = aedis::resp3::node<std::string>;

//-------------------------------------------------------------------

template <class Result>
struct expect {
   std::string in;
   Result expected;
   std::string name;
   boost::system::error_condition ec;
};

template <class Result>
void test_sync(expect<Result> e, net::io_context& ioc)
{
   std::string rbuffer;
   test_stream ts {ioc};
   ts.append(e.in);
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
   auto ex = co_await net::this_coro::executor;
   std::string rbuffer;
   test_stream ts {ex};
   ts.append(e.in);
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
   auto const in16 = expect<std::optional<int>>{":11\r\n", ok, "number.optional.int"};

   // Transaction.
   auto const in17 = expect<std::tuple<int>>{"*1\r\n:11\r\n", std::tuple<int>{11}, "number.tuple.int"};

   // Error
   auto const in04 = expect<std::optional<int>>{"%11\r\n", std::optional<int>{}, "number.optional.int", aedis::adapter::make_error_condition(aedis::adapter::error::expects_simple_type)};
   auto const in06 = expect<std::set<std::string>>{":11\r\n", std::set<std::string>{}, "number.optional.int", aedis::adapter::make_error_condition(aedis::adapter::error::expects_set_aggregate)};
   auto const in07 = expect<std::unordered_set<std::string>>{":11\r\n", std::unordered_set<std::string>{}, "number.optional.int", aedis::adapter::make_error_condition(aedis::adapter::error::expects_set_aggregate)};
   auto const in08 = expect<std::map<std::string, std::string>>{":11\r\n", std::map<std::string, std::string>{}, "number.optional.int", aedis::adapter::make_error_condition(aedis::adapter::error::expects_map_like_aggregate)};
   auto const in09 = expect<std::unordered_map<std::string, std::string>>{":11\r\n", std::unordered_map<std::string, std::string>{}, "number.optional.int", aedis::adapter::make_error_condition(aedis::adapter::error::expects_map_like_aggregate)};
   auto const in10 = expect<std::list<std::string>>{":11\r\n", std::list<std::string>{}, "number.optional.int", aedis::adapter::make_error_condition(aedis::adapter::error::expects_aggregate)};

   test_sync(in04, ioc);
   test_sync(in06, ioc);
   test_sync(in07, ioc);
   test_sync(in08, ioc);
   test_sync(in09, ioc);
   test_sync(in10, ioc);
   test_sync(in12, ioc);
   test_sync(in13, ioc);
   test_sync(in15, ioc);
   test_sync(in16, ioc);
   test_sync(in17, ioc);

   co_spawn(ioc, test_async(in04), net::detached);
   co_spawn(ioc, test_async(in06), net::detached);
   co_spawn(ioc, test_async(in07), net::detached);
   co_spawn(ioc, test_async(in08), net::detached);
   co_spawn(ioc, test_async(in09), net::detached);
   co_spawn(ioc, test_async(in10), net::detached);
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
   auto const in08 = expect<node_type>{"#f\r\n", node_type{resp3::type::boolean, 1UL, 0UL, {"f"}}, "bool.node (false)"};
   auto const in09 = expect<node_type>{"#t\r\n", node_type{resp3::type::boolean, 1UL, 0UL, {"t"}}, "bool.node (true)"};
   auto const in10 = expect<bool>{"#t\r\n", bool{true}, "bool.bool (true)"};
   auto const in11 = expect<bool>{"#f\r\n", bool{false}, "bool.bool (true)"};
   auto const in13 = expect<std::optional<bool>>{"#t\r\n", ok, "optional.int"};

   // Error
   auto const in01 = expect<std::optional<bool>>{"#11\r\n", std::optional<bool>{}, "bool.error", aedis::resp3::make_error_condition(aedis::resp3::error::unexpected_bool_value)};
   auto const in03 = expect<std::set<int>>{"#t\r\n", std::set<int>{}, "bool.error", aedis::adapter::make_error_condition(aedis::adapter::error::expects_set_aggregate)};
   auto const in04 = expect<std::unordered_set<int>>{"#t\r\n", std::unordered_set<int>{}, "bool.error", aedis::adapter::make_error_condition(aedis::adapter::error::expects_set_aggregate)};
   auto const in05 = expect<std::map<int, int>>{"#t\r\n", std::map<int, int>{}, "bool.error", aedis::adapter::make_error_condition(aedis::adapter::error::expects_map_like_aggregate)};
   auto const in06 = expect<std::unordered_map<int, int>>{"#t\r\n", std::unordered_map<int, int>{}, "bool.error", aedis::adapter::make_error_condition(aedis::adapter::error::expects_map_like_aggregate)};

   test_sync(in01, ioc);
   test_sync(in03, ioc);
   test_sync(in04, ioc);
   test_sync(in05, ioc);
   test_sync(in06, ioc);
   test_sync(in08, ioc);
   test_sync(in09, ioc);
   test_sync(in10, ioc);
   test_sync(in11, ioc);

   co_spawn(ioc, test_async(in01), net::detached);
   co_spawn(ioc, test_async(in03), net::detached);
   co_spawn(ioc, test_async(in04), net::detached);
   co_spawn(ioc, test_async(in05), net::detached);
   co_spawn(ioc, test_async(in06), net::detached);
   co_spawn(ioc, test_async(in08), net::detached);
   co_spawn(ioc, test_async(in09), net::detached);
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
   , {aedis::resp3::type::streamed_string_part, 1, 1, ""}
   };

   std::vector<node_type> e1b { {resp3::type::streamed_string_part, 1UL, 1UL, {}} };

   auto const in01 = expect<std::vector<node_type>>{wire, e1a, "streamed_string.node"};
   auto const in03 = expect<std::string>{wire, std::string{"Hello word"}, "streamed_string.string"};
   auto const in02 = expect<std::vector<node_type>>{"$?\r\n;0\r\n", e1b, "streamed_string.node.empty"};

   test_sync(in01, ioc);
   test_sync(in02, ioc);
   test_sync(in03, ioc);

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

   test_sync(in01, ioc);
   test_sync(in02, ioc);

   co_spawn(ioc, test_async(in01), net::detached);
   co_spawn(ioc, test_async(in02), net::detached);
}

void test_map(net::io_context& ioc)
{
   using map_type = std::map<std::string, std::string>;
   using mmap_type = std::multimap<std::string, std::string>;
   using umap_type = std::unordered_map<std::string, std::string>;
   using mumap_type = std::unordered_multimap<std::string, std::string>;
   using vec_type = std::vector<std::string>;
   using op_map_type = std::optional<std::map<std::string, std::string>>;
   using op_vec_type = std::optional<std::vector<std::string>>;
   using tuple_type = std::tuple<std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string>;

   std::string const wire = "%4\r\n$4\r\nkey1\r\n$6\r\nvalue1\r\n$4\r\nkey2\r\n$6\r\nvalue2\r\n$4\r\nkey3\r\n$6\r\nvalue3\r\n$4\r\nkey3\r\n$6\r\nvalue3\r\n";

   std::vector<node_type> expected_1a
   { {resp3::type::map,         4UL, 0UL, {}}
   , {resp3::type::blob_string, 1UL, 1UL, {"key1"}}
   , {resp3::type::blob_string, 1UL, 1UL, {"value1"}}
   , {resp3::type::blob_string, 1UL, 1UL, {"key2"}}
   , {resp3::type::blob_string, 1UL, 1UL, {"value2"}}
   , {resp3::type::blob_string, 1UL, 1UL, {"key3"}}
   , {resp3::type::blob_string, 1UL, 1UL, {"value3"}}
   , {resp3::type::blob_string, 1UL, 1UL, {"key3"}}
   , {resp3::type::blob_string, 1UL, 1UL, {"value3"}}
   };

   map_type expected_1b
   { {"key1", "value1"}
   , {"key2", "value2"}
   , {"key3", "value3"}
   };

   umap_type e1g
   { {"key1", "value1"}
   , {"key2", "value2"}
   , {"key3", "value3"}
   };

   mmap_type e1k
   { {"key1", "value1"}
   , {"key2", "value2"}
   , {"key3", "value3"}
   , {"key3", "value3"}
   };

   mumap_type e1l
   { {"key1", "value1"}
   , {"key2", "value2"}
   , {"key3", "value3"}
   , {"key3", "value3"}
   };

   std::vector<std::string> expected_1c
   { "key1", "value1"
   , "key2", "value2"
   , "key3", "value3"
   , "key3", "value3"
   };

   op_map_type expected_1d;
   expected_1d = expected_1b;

   op_vec_type expected_1e;
   expected_1e = expected_1c;

   tuple_type e1f
   { std::string{"key1"}, std::string{"value1"}
   , std::string{"key2"}, std::string{"value2"}
   , std::string{"key3"}, std::string{"value3"}
   , std::string{"key3"}, std::string{"value3"}
   };

   auto const in00 = expect<std::vector<node_type>>{wire, expected_1a, "map.node"};
   auto const in01 = expect<map_type>{"%0\r\n", map_type{}, "map.map.empty"};
   auto const in02 = expect<map_type>{wire, expected_1b, "map.map"};
   auto const in03 = expect<mmap_type>{wire, e1k, "map.multimap"};
   auto const in04 = expect<umap_type>{wire, e1g, "map.unordered_map"};
   auto const in05 = expect<mumap_type>{wire, e1l, "map.unordered_multimap"};
   auto const in06 = expect<vec_type>{wire, expected_1c, "map.vector"};
   auto const in07 = expect<op_map_type>{wire, expected_1d, "map.optional.map"};
   auto const in08 = expect<op_vec_type>{wire, expected_1e, "map.optional.vector"};
   auto const in09 = expect<std::tuple<op_map_type>>{"*1\r\n" + wire, std::tuple<op_map_type>{expected_1d}, "map.transaction.optional.map"};
   auto const in10 = expect<int>{"%11\r\n", int{}, "map.invalid.int", aedis::adapter::make_error_condition(aedis::adapter::error::expects_simple_type)};
   auto const in11 = expect<tuple_type>{wire, e1f, "map.tuple"};

   test_sync(in00, ioc);
   test_sync(in01, ioc);
   test_sync(in02, ioc);
   test_sync(in03, ioc);
   test_sync(in04, ioc);
   test_sync(in05, ioc);
   test_sync(in07, ioc);
   test_sync(in08, ioc);
   test_sync(in09, ioc);
   test_sync(in00, ioc);
   test_sync(in11, ioc);

   co_spawn(ioc, test_async(in00), net::detached);
   co_spawn(ioc, test_async(in01), net::detached);
   co_spawn(ioc, test_async(in02), net::detached);
   co_spawn(ioc, test_async(in03), net::detached);
   co_spawn(ioc, test_async(in04), net::detached);
   co_spawn(ioc, test_async(in05), net::detached);
   co_spawn(ioc, test_async(in07), net::detached);
   co_spawn(ioc, test_async(in08), net::detached);
   co_spawn(ioc, test_async(in09), net::detached);
   co_spawn(ioc, test_async(in10), net::detached);
   co_spawn(ioc, test_async(in11), net::detached);
}

void test_attribute(net::io_context& ioc)
{
   char const* wire = "|1\r\n+key-popularity\r\n%2\r\n$1\r\na\r\n,0.1923\r\n$1\r\nb\r\n,0.0012\r\n";

   std::vector<node_type> e1a
      { {resp3::type::attribute,     1UL, 0UL, {}}
      , {resp3::type::simple_string, 1UL, 1UL, "key-popularity"}
      , {resp3::type::map,           2UL, 1UL, {}}
      , {resp3::type::blob_string,   1UL, 2UL, "a"}
      , {resp3::type::doublean,      1UL, 2UL, "0.1923"}
      , {resp3::type::blob_string,   1UL, 2UL, "b"}
      , {resp3::type::doublean,      1UL, 2UL, "0.0012"}
      };

   std::vector<node_type> e1b;

   auto const in01 = expect<std::vector<node_type>>{wire, e1a, "attribute.node"};
   auto const in02 = expect<std::vector<node_type>>{"|0\r\n", e1b, "attribute.node.empty"};

   test_sync(in01, ioc);
   test_sync(in02, ioc);

   co_spawn(ioc, test_async(in01), net::detached);
   co_spawn(ioc, test_async(in02), net::detached);
}

void test_array(net::io_context& ioc)
{
   char const* wire = "*3\r\n$2\r\n11\r\n$2\r\n22\r\n$1\r\n3\r\n";

   std::vector<node_type> e1a
      { {resp3::type::array,       3UL, 0UL, {}}
      , {resp3::type::blob_string, 1UL, 1UL, {"11"}}
      , {resp3::type::blob_string, 1UL, 1UL, {"22"}}
      , {resp3::type::blob_string, 1UL, 1UL, {"3"}}
      };

   std::vector<int> const e1b{11, 22, 3};
   std::vector<std::string> const e1c{"11", "22", "3"};
   std::vector<std::string> const e1d{};
   std::vector<node_type> const e1e{{resp3::type::array, 0UL, 0UL, {}}};
   std::array<int, 3> const e1f{11, 22, 3};
   std::list<int> const e1g{11, 22, 3};
   std::deque<int> const e1h{11, 22, 3};

   auto const in01 = expect<std::vector<node_type>>{wire, e1a, "array.node"};
   auto const in02 = expect<std::vector<int>>{wire, e1b, "array.int"};
   auto const in03 = expect<std::vector<node_type>>{"*0\r\n", e1e, "array.node.empty"};
   auto const in04 = expect<std::vector<std::string>>{"*0\r\n", e1d, "array.string.empty"};
   auto const in05 = expect<std::vector<std::string>>{wire, e1c, "array.string"};
   auto const in06 = expect<std::array<int, 3>>{wire, e1f, "array.array"};
   auto const in07 = expect<std::list<int>>{wire, e1g, "array.list"};
   auto const in08 = expect<std::deque<int>>{wire, e1h, "array.deque"};

   test_sync(in01, ioc);
   test_sync(in02, ioc);
   test_sync(in03, ioc);
   test_sync(in04, ioc);
   test_sync(in05, ioc);
   test_sync(in06, ioc);
   test_sync(in07, ioc);
   test_sync(in08, ioc);

   co_spawn(ioc, test_async(in01), net::detached);
   co_spawn(ioc, test_async(in02), net::detached);
   co_spawn(ioc, test_async(in03), net::detached);
   co_spawn(ioc, test_async(in04), net::detached);
   co_spawn(ioc, test_async(in05), net::detached);
   co_spawn(ioc, test_async(in06), net::detached);
   co_spawn(ioc, test_async(in07), net::detached);
   co_spawn(ioc, test_async(in08), net::detached);
}

void test_set(net::io_context& ioc)
{
   using set_type = std::set<std::string>;
   using mset_type = std::multiset<std::string>;
   using uset_type = std::unordered_set<std::string>;
   using muset_type = std::unordered_multiset<std::string>;
   using vec_type = std::vector<std::string>;
   using op_vec_type = std::optional<std::vector<std::string>>;

   std::string const wire = "~6\r\n+orange\r\n+apple\r\n+one\r\n+two\r\n+three\r\n+orange\r\n";
   std::vector<node_type> const expected1a
   { {resp3::type::set,            6UL, 0UL, {}}
   , {resp3::type::simple_string,  1UL, 1UL, {"orange"}}
   , {resp3::type::simple_string,  1UL, 1UL, {"apple"}}
   , {resp3::type::simple_string,  1UL, 1UL, {"one"}}
   , {resp3::type::simple_string,  1UL, 1UL, {"two"}}
   , {resp3::type::simple_string,  1UL, 1UL, {"three"}}
   , {resp3::type::simple_string,  1UL, 1UL, {"orange"}}
   };

   mset_type const e1f{"apple", "one", "orange", "orange", "three", "two"};
   uset_type const e1c{"apple", "one", "orange", "three", "two"};
   muset_type const e1g{"apple", "one", "orange", "orange", "three", "two"};
   vec_type const e1d = {"orange", "apple", "one", "two", "three", "orange"};
   op_vec_type expected_1e;
   expected_1e = e1d;

   auto const in00 = expect<std::vector<node_type>>{wire, expected1a, "set.node"};
   auto const in01 = expect<std::vector<node_type>>{"~0\r\n", std::vector<node_type>{ {resp3::type::set,  0UL, 0UL, {}} }, "set.node (empty)"};
   auto const in02 = expect<set_type>{wire, set_type{"apple", "one", "orange", "three", "two"}, "set.set"};
   auto const in03 = expect<mset_type>{wire, e1f, "set.multiset"};
   auto const in04 = expect<vec_type>{wire, e1d, "set.vector "};
   auto const in05 = expect<op_vec_type>{wire, expected_1e, "set.vector "};
   auto const in06 = expect<uset_type>{wire, e1c, "set.unordered_set"};
   auto const in07 = expect<muset_type>{wire, e1g, "set.unordered_multiset"};
   auto const in08 = expect<std::tuple<uset_type>>{"*1\r\n" + wire, std::tuple<uset_type>{e1c}, "set.tuple"};

   test_sync(in00, ioc);
   test_sync(in01, ioc);
   test_sync(in02, ioc);
   test_sync(in03, ioc);
   test_sync(in04, ioc);
   test_sync(in05, ioc);
   test_sync(in06, ioc);
   test_sync(in07, ioc);
   test_sync(in08, ioc);

   co_spawn(ioc, test_async(in00), net::detached);
   co_spawn(ioc, test_async(in01), net::detached);
   co_spawn(ioc, test_async(in02), net::detached);
   co_spawn(ioc, test_async(in03), net::detached);
   co_spawn(ioc, test_async(in04), net::detached);
   co_spawn(ioc, test_async(in05), net::detached);
   co_spawn(ioc, test_async(in06), net::detached);
   co_spawn(ioc, test_async(in07), net::detached);
   co_spawn(ioc, test_async(in08), net::detached);
}

void test_simple_error(net::io_context& ioc)
{
   auto const in01 = expect<node_type>{"-Error\r\n", node_type{resp3::type::simple_error, 1UL, 0UL, {"Error"}}, "simple_error.node"};
   auto const in02 = expect<node_type>{"-\r\n", node_type{resp3::type::simple_error, 1UL, 0UL, {""}}, "simple_error.node.empty"};

   test_sync(in01, ioc);
   test_sync(in02, ioc);
   co_spawn(ioc, test_async(in01), net::detached);
   co_spawn(ioc, test_async(in02), net::detached);
}

void test_blob_string(net::io_context& ioc)
{
   std::string str(100000, 'a');
   str[1000] = '\r';
   str[1001] = '\n';

   std::string wire;
   wire += '$';
   wire += std::to_string(std::size(str));
   wire += "\r\n";
   wire += str;
   wire += "\r\n";

   auto const in01 = expect<node_type>{"$2\r\nhh\r\n", node_type{resp3::type::blob_string, 1UL, 0UL, {"hh"}}, "blob_string.node"};
   auto const in02 = expect<node_type>{"$26\r\nhhaa\aaaa\raaaaa\r\naaaaaaaaaa\r\n", node_type{resp3::type::blob_string, 1UL, 0UL, {"hhaa\aaaa\raaaaa\r\naaaaaaaaaa"}}, "blob_string.node (with separator)"};
   auto const in03 = expect<node_type>{"$0\r\n\r\n", node_type{resp3::type::blob_string, 1UL, 0UL, {}}, "blob_string.node.empty"};
   auto const in04 = expect<node_type>{wire, node_type{resp3::type::blob_string, 1UL, 0UL, {str}}, "blob_string.node (long string)"};

   test_sync(in01, ioc);
   test_sync(in02, ioc);
   test_sync(in03, ioc);
   test_sync(in04, ioc);

   co_spawn(ioc, test_async(in01), net::detached);
   co_spawn(ioc, test_async(in02), net::detached);
   co_spawn(ioc, test_async(in03), net::detached);
   co_spawn(ioc, test_async(in04), net::detached);
}

void test_double(net::io_context& ioc)
{
   auto const in01 = expect<node_type>{",1.23\r\n", node_type{resp3::type::doublean, 1UL, 0UL, {"1.23"}}, "double.node"};
   auto const in02 = expect<node_type>{",inf\r\n", node_type{resp3::type::doublean, 1UL, 0UL, {"inf"}}, "double.node (inf)"};
   auto const in03 = expect<node_type>{",-inf\r\n", node_type{resp3::type::doublean, 1UL, 0UL, {"-inf"}}, "double.node (-inf)"};

   test_sync(in01, ioc);
   test_sync(in02, ioc);
   test_sync(in03, ioc);

   co_spawn(ioc, test_async(in01), net::detached);
   co_spawn(ioc, test_async(in02), net::detached);
   co_spawn(ioc, test_async(in03), net::detached);
}

void test_blob_error(net::io_context& ioc)
{
   auto const in01 = expect<node_type>{"!21\r\nSYNTAX invalid syntax\r\n", node_type{resp3::type::blob_error, 1UL, 0UL, {"SYNTAX invalid syntax"}}, "blob_error"};
   auto const in02 = expect<node_type>{"!0\r\n\r\n", node_type{resp3::type::blob_error, 1UL, 0UL, {}}, "blob_error.empty"};

   test_sync(in01, ioc);
   test_sync(in02, ioc);

   co_spawn(ioc, test_async(in01), net::detached);
   co_spawn(ioc, test_async(in02), net::detached);
}

void test_verbatim_string(net::io_context& ioc)
{
   auto const in01 = expect<node_type>{"=15\r\ntxt:Some string\r\n", node_type{resp3::type::verbatim_string, 1UL, 0UL, {"txt:Some string"}}, "verbatim_string"};
   auto const in02 = expect<node_type>{"=0\r\n\r\n", node_type{resp3::type::verbatim_string, 1UL, 0UL, {}}, "verbatim_string.empty"};

   test_sync(in01, ioc);
   test_sync(in02, ioc);

   co_spawn(ioc, test_async(in01), net::detached);
   co_spawn(ioc, test_async(in02), net::detached);
}

void test_big_number(net::io_context& ioc)
{
   auto const in01 = expect<node_type>{"(3492890328409238509324850943850943825024385\r\n", node_type{resp3::type::big_number, 1UL, 0UL, {"3492890328409238509324850943850943825024385"}}, "big_number.node"};
   auto const in02 = expect<int>{"(\r\n", int{}, "big_number.error (empty field)", aedis::resp3::make_error_condition(aedis::resp3::error::empty_field)};

   test_sync(in01, ioc);
   test_sync(in02, ioc);

   co_spawn(ioc, test_async(in01), net::detached);
   co_spawn(ioc, test_async(in02), net::detached);
}

void test_simple_string(net::io_context& ioc)
{
   std::optional<std::string> ok1, ok2;
   ok1 = "OK";
   ok2 = "";

   test_sync(expect<std::string>{"+OK\r\n", std::string{"OK"}, "simple_string.string"}, ioc);
   test_sync(expect<std::string>{"+\r\n", std::string{""}, "simple_string.string.empty"}, ioc);
   test_sync(expect<std::optional<std::string>>{"+OK\r\n", std::optional<std::string>{"OK"}, "simple_string.optional"}, ioc);
   test_sync(expect<std::optional<std::string>>{"+\r\n", std::optional<std::string>{""}, "simple_string.optional.empty"}, ioc);
   co_spawn(ioc, test_async(expect<std::string>{"+OK\r\n", std::string{"OK"}, "simple_string.async.string"}), net::detached);
   co_spawn(ioc, test_async(expect<std::string>{"+\r\n", std::string{""}, "simple_string.async.string.empty"}), net::detached);
   co_spawn(ioc, test_async(expect<std::optional<std::string>>{"+OK\r\n", ok1, "simple_string.async.string.optional"}), net::detached);
   co_spawn(ioc, test_async(expect<std::optional<std::string>>{"+\r\n", ok2, "simple_string.async.string.optional.empty"}), net::detached);
}

void test_resp3(net::io_context& ioc)
{
   auto const in01 = expect<int>{"s11\r\n", int{}, "number.error", aedis::resp3::make_error_condition(aedis::resp3::error::invalid_type)};
   auto const in02 = expect<int>{":adf\r\n", int{11}, "number.int", boost::system::errc::errc_t::invalid_argument};
   auto const in03 = expect<int>{":\r\n", int{}, "number.error (empty field)", aedis::resp3::make_error_condition(aedis::resp3::error::empty_field)};
   auto const in04 = expect<std::optional<bool>>{"#\r\n", std::optional<bool>{}, "bool.error", aedis::resp3::make_error_condition(aedis::resp3::error::empty_field)};
   auto const in05 = expect<std::string>{",\r\n", std::string{}, "double.error (empty field)", aedis::resp3::make_error_condition(aedis::resp3::error::empty_field)};

   test_sync(in01, ioc);
   test_sync(in02, ioc);
   test_sync(in03, ioc);
   test_sync(in04, ioc);
   test_sync(in05, ioc);

   co_spawn(ioc, test_async(in01), net::detached);
   co_spawn(ioc, test_async(in02), net::detached);
   co_spawn(ioc, test_async(in03), net::detached);
   co_spawn(ioc, test_async(in04), net::detached);
   co_spawn(ioc, test_async(in05), net::detached);
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

   test_sync(in01, ioc);
   test_sync(in02, ioc);
   test_sync(in03, ioc);
   test_sync(in04, ioc);
   test_sync(in05, ioc);
   test_sync(in06, ioc);
   test_sync(in07, ioc);
   test_sync(in08, ioc);
   test_sync(in09, ioc);

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

   // Simple types.
   test_simple_string(ioc);
   test_simple_error(ioc);
   test_blob_string(ioc);
   test_blob_error(ioc);
   test_number(ioc);
   test_double(ioc);
   test_bool(ioc);
   test_null(ioc);
   test_big_number(ioc);
   test_verbatim_string(ioc);

   // Aggregates.
   test_array(ioc);
   test_set(ioc);
   test_map(ioc);
   test_push(ioc);
   test_streamed_string(ioc);

   // RESP3
   test_resp3(ioc);

   ioc.run();
}

