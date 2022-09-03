/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <map>
#include <iostream>
#include <optional>
#include <sstream>

#include <boost/system/errc.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/beast/_experimental/test/stream.hpp>
#define BOOST_TEST_MODULE low level
#include <boost/test/included/unit_test.hpp>

#include <aedis.hpp>
#include <aedis/src.hpp>

// TODO: Test with empty strings.

namespace net = boost::asio;
namespace resp3 = aedis::resp3;

using test_stream = boost::beast::test::stream;
using aedis::adapter::adapt2;
using node_type = aedis::resp3::node<std::string>;
using vec_node_type = std::vector<node_type>;

//-------------------------------------------------------------------

template <class Result>
struct expect {
   std::string in;
   Result expected;
   std::string name;
   boost::system::error_code ec{};
};

template <class Result>
auto make_expected(std::string in, Result expected, std::string name, boost::system::error_code ec = {})
{
   return expect<Result>{in, expected, name, ec};
}

template <class Result>
void test_sync(net::any_io_executor ex, expect<Result> e)
{
   std::cout << e.name << std::endl;
   std::string rbuffer;
   test_stream ts {ex};
   ts.append(e.in);
   Result result;
   boost::system::error_code ec;
   resp3::read(ts, net::dynamic_buffer(rbuffer), adapt2(result), ec);
   BOOST_CHECK_EQUAL(ec, e.ec);
   if (e.ec)
      return;
   BOOST_TEST(rbuffer.empty());
   auto const res = result == e.expected;
   BOOST_TEST(res);
}

template <class Result>
class async_test: public std::enable_shared_from_this<async_test<Result>> {
private:
   std::string rbuffer_;
   test_stream ts_;
   expect<Result> data_;
   Result result_;

public:
   async_test(net::any_io_executor ex, expect<Result> e)
   : ts_{ex}
   , data_{e}
   {
      ts_.append(e.in);
   }

   void run()
   {
      auto self = this->shared_from_this();
      auto f = [self](auto ec, auto)
      {
         BOOST_CHECK_EQUAL(ec, self->data_.ec);
         if (self->data_.ec)
            return;
         BOOST_TEST(self->rbuffer_.empty());
         auto const res = self->result_ == self->data_.expected;
         BOOST_TEST(res);
      };

      resp3::async_read(
         ts_,
         net::dynamic_buffer(rbuffer_),
         adapt2(result_),
         f);
   }
};

template <class Result>
void test_async(net::any_io_executor ex, expect<Result> e)
{
   std::make_shared<async_test<Result>>(ex, e)->run();
}

boost::optional<int> op_int_ok = 11;
boost::optional<bool> op_bool_ok = true;

std::string const streamed_string_wire = "$?\r\n;4\r\nHell\r\n;5\r\no wor\r\n;1\r\nd\r\n;0\r\n";
std::vector<node_type> streamed_string_e1
{ {aedis::resp3::type::streamed_string_part, 1, 1, "Hell"}
, {aedis::resp3::type::streamed_string_part, 1, 1, "o wor"}
, {aedis::resp3::type::streamed_string_part, 1, 1, "d"}
, {aedis::resp3::type::streamed_string_part, 1, 1, ""}
};

std::vector<node_type> streamed_string_e2 { {resp3::type::streamed_string_part, 1UL, 1UL, {}} };


#define NUMBER_TEST_CONDITIONS(test) \
   test(ex, make_expected("#11\r\n",       boost::optional<bool>{}, "bool.error", aedis::error::unexpected_bool_value)); \
   test(ex, make_expected("#f\r\n",        bool{false},             "bool.bool (true)")); \
   test(ex, make_expected("#f\r\n",        node_type{resp3::type::boolean, 1UL, 0UL, {"f"}}, "bool.node (false)")); \
   test(ex, make_expected("#t\r\n",        bool{true}, "bool.bool (true)")); \
   test(ex, make_expected("#t\r\n",        node_type{resp3::type::boolean, 1UL, 0UL, {"t"}}, "bool.node (true)")); \
   test(ex, make_expected("#t\r\n",        op_bool_ok, "optional.int")); \
   test(ex, make_expected("#t\r\n",        std::map<int, int>{}, "bool.error", aedis::error::expects_resp3_map)); \
   test(ex, make_expected("#t\r\n",        std::set<int>{}, "bool.error", aedis::error::expects_resp3_set)); \
   test(ex, make_expected("#t\r\n",        std::unordered_map<int, int>{}, "bool.error", aedis::error::expects_resp3_map)); \
   test(ex, make_expected("#t\r\n",        std::unordered_set<int>{}, "bool.error", aedis::error::expects_resp3_set)); \
   test(ex, make_expected("$?\r\n;0\r\n",  streamed_string_e2, "streamed_string.node.empty")); \
   test(ex, make_expected("%11\r\n",       boost::optional<int>{}, "number.optional.int.error", aedis::error::expects_resp3_simple_type));; \
   test(ex, make_expected("*1\r\n:11\r\n", std::tuple<int>{11}, "number.tuple.int")); \
   test(ex, make_expected(":-3\r\n",       node_type{resp3::type::number, 1UL, 0UL, {"-3"}}, "number.node (negative)")); \
   test(ex, make_expected(":11\r\n",       int{11}, "number.int")); \
   test(ex, make_expected(":11\r\n",       op_int_ok, "number.optional.int")); \
   test(ex, make_expected(":11\r\n",       std::list<std::string>{}, "number.optional.int", aedis::error::expects_resp3_aggregate)); \
   test(ex, make_expected(":11\r\n",       std::map<std::string, std::string>{}, "number.optional.int", aedis::error::expects_resp3_map)); \
   test(ex, make_expected(":11\r\n",       std::set<std::string>{}, "number.optional.int", aedis::error::expects_resp3_set)); \
   test(ex, make_expected(":11\r\n",       std::unordered_map<std::string, std::string>{}, "number.optional.int", aedis::error::expects_resp3_map)); \
   test(ex, make_expected(":11\r\n",       std::unordered_set<std::string>{}, "number.optional.int", aedis::error::expects_resp3_set)); \
   test(ex, make_expected(":3\r\n",        node_type{resp3::type::number, 1UL, 0UL, {"3"}}, "number.node (positive)")); \
   test(ex, make_expected("_\r\n",         int{0}, "number.int.error.null", aedis::error::resp3_null)); \
   test(ex, make_expected(streamed_string_wire, std::string{"Hello word"}, "streamed_string.string")); \
   test(ex, make_expected(streamed_string_wire, int{}, "streamed_string.string", aedis::error::not_a_number)); \
   test(ex, make_expected(streamed_string_wire, streamed_string_e1, "streamed_string.node")); \

BOOST_AUTO_TEST_CASE(test_push)
{
   net::io_context ioc;
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

   auto ex = ioc.get_executor();

   test_sync(ex, in01);
   test_sync(ex, in02);

   test_async(ex, in01);
   test_async(ex, in02);
   ioc.run();
}

BOOST_AUTO_TEST_CASE(test_map)
{
   net::io_context ioc;
   using map_type = std::map<std::string, std::string>;
   using mmap_type = std::multimap<std::string, std::string>;
   using umap_type = std::unordered_map<std::string, std::string>;
   using mumap_type = std::unordered_multimap<std::string, std::string>;
   using vec_type = std::vector<std::string>;
   using op_map_type = boost::optional<std::map<std::string, std::string>>;
   using op_vec_type = boost::optional<std::vector<std::string>>;
   using tuple_type = std::tuple<std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string>;

   std::string const wire2 = "*3\r\n$2\r\n11\r\n$2\r\n22\r\n$1\r\n3\r\n";
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
   auto const in10 = expect<int>{"%11\r\n", int{}, "map.invalid.int", aedis::error::expects_resp3_simple_type};
   auto const in11 = expect<tuple_type>{wire, e1f, "map.tuple"};
   auto const in12 = expect<map_type>{wire2, map_type{}, "map.error", aedis::error::expects_resp3_map};
   auto const in13 = expect<map_type>{"_\r\n", map_type{}, "map.null", aedis::error::resp3_null};

   auto ex = ioc.get_executor();

   test_sync(ex, in00);
   test_sync(ex, in01);
   test_sync(ex, in02);
   test_sync(ex, in03);
   test_sync(ex, in04);
   test_sync(ex, in05);
   test_sync(ex, in06);
   test_sync(ex, in07);
   test_sync(ex, in08);
   test_sync(ex, in09);
   test_sync(ex, in10);
   test_sync(ex, in11);
   test_sync(ex, in12);
   test_sync(ex, in13);

   test_async(ex, in00);
   test_async(ex, in01);
   test_async(ex, in02);
   test_async(ex, in03);
   test_async(ex, in04);
   test_async(ex, in05);
   test_async(ex, in06);
   test_async(ex, in07);
   test_async(ex, in08);
   test_async(ex, in09);
   test_async(ex, in10);
   test_async(ex, in11);
   test_async(ex, in12);
   test_async(ex, in13);
   ioc.run();
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

   auto ex = ioc.get_executor();

   test_sync(ex, in01);
   test_sync(ex, in02);

   test_async(ex, in01);
   test_async(ex, in02);
}

BOOST_AUTO_TEST_CASE(test_array)
{
   using tuple_type = std::tuple<int, int>;
   using array_type = std::array<int, 3>;
   using array_type2 = std::array<int, 1>;

   net::io_context ioc;
   char const* wire = "*3\r\n$2\r\n11\r\n$2\r\n22\r\n$1\r\n3\r\n";
   char const* wire_nested = "*1\r\n*1\r\n$2\r\nab\r\n";
   char const* wire_nested2 = "*1\r\n*1\r\n*1\r\n*1\r\n*1\r\n*1\r\na\r\n";

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
   array_type const e1f{11, 22, 3};
   std::list<int> const e1g{11, 22, 3};
   std::deque<int> const e1h{11, 22, 3};

   auto const in01 = expect<std::vector<node_type>>{wire, e1a, "array.node"};
   auto const in02 = expect<std::vector<int>>{wire, e1b, "array.int"};
   auto const in03 = expect<std::vector<node_type>>{"*0\r\n", e1e, "array.node.empty"};
   auto const in04 = expect<std::vector<std::string>>{"*0\r\n", e1d, "array.string.empty"};
   auto const in05 = expect<std::vector<std::string>>{wire, e1c, "array.string"};
   auto const in06 = expect<array_type>{wire, e1f, "array.array"};
   auto const in07 = expect<std::list<int>>{wire, e1g, "array.list"};
   auto const in08 = expect<std::deque<int>>{wire, e1h, "array.deque"};
   auto const in09 = expect<std::vector<int>>{"_\r\n", std::vector<int>{}, "array.vector", aedis::error::resp3_null};
   auto const in10 = expect<std::list<int>>{"_\r\n", std::list<int>{}, "array.list", aedis::error::resp3_null};
   auto const in11 = expect<array_type>{"_\r\n", array_type{}, "array.null", aedis::error::resp3_null};
   auto const in12 = expect<tuple_type>{wire, tuple_type{}, "array.list", aedis::error::incompatible_size};
   auto const in13 = expect<array_type2>{wire_nested, array_type2{}, "array.nested", aedis::error::nested_aggregate_not_supported};
   auto const in14 = expect<array_type2>{wire, array_type2{}, "array.null", aedis::error::incompatible_size};
   auto const in15 = expect<array_type2>{":3\r\n", array_type2{}, "array.array", aedis::error::expects_resp3_aggregate};
   auto const in16 = expect<vec_node_type>{wire_nested2, vec_node_type{}, "array.depth.exceeds", aedis::error::exceeeds_max_nested_depth};

   auto ex = ioc.get_executor();

   test_sync(ex, in01);
   test_sync(ex, in02);
   test_sync(ex, in03);
   test_sync(ex, in04);
   test_sync(ex, in05);
   test_sync(ex, in06);
   test_sync(ex, in07);
   test_sync(ex, in08);
   test_sync(ex, in09);
   test_sync(ex, in10);
   test_sync(ex, in11);
   test_sync(ex, in12);
   test_sync(ex, in13);
   test_sync(ex, in14);
   test_sync(ex, in15);
   test_sync(ex, in16);

   test_async(ex, in01);
   test_async(ex, in02);
   test_async(ex, in03);
   test_async(ex, in04);
   test_async(ex, in05);
   test_async(ex, in06);
   test_async(ex, in07);
   test_async(ex, in08);
   test_async(ex, in09);
   test_async(ex, in10);
   test_async(ex, in11);
   test_async(ex, in12);
   test_async(ex, in13);
   test_async(ex, in14);
   test_async(ex, in15);
   test_async(ex, in16);

   ioc.run();
}

BOOST_AUTO_TEST_CASE(test_set)
{
   net::io_context ioc;
   using set_type = std::set<std::string>;
   using mset_type = std::multiset<std::string>;
   using uset_type = std::unordered_set<std::string>;
   using muset_type = std::unordered_multiset<std::string>;
   using vec_type = std::vector<std::string>;
   using op_vec_type = boost::optional<std::vector<std::string>>;

   std::string const wire2 = "*3\r\n$2\r\n11\r\n$2\r\n22\r\n$1\r\n3\r\n";
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
   auto const in09 = expect<set_type>{wire2, set_type{}, "set.error", aedis::error::expects_resp3_set};

   auto ex = ioc.get_executor();

   test_sync(ex, in00);
   test_sync(ex, in01);
   test_sync(ex, in02);
   test_sync(ex, in03);
   test_sync(ex, in04);
   test_sync(ex, in05);
   test_sync(ex, in06);
   test_sync(ex, in07);
   test_sync(ex, in08);
   test_sync(ex, in09);

   test_async(ex, in00);
   test_async(ex, in01);
   test_async(ex, in02);
   test_async(ex, in03);
   test_async(ex, in04);
   test_async(ex, in05);
   test_async(ex, in06);
   test_async(ex, in07);
   test_async(ex, in08);
   test_async(ex, in09);
   ioc.run();
}

BOOST_AUTO_TEST_CASE(test_simple_error)
{
   net::io_context ioc;
   auto const in01 = expect<node_type>{"-Error\r\n", node_type{resp3::type::simple_error, 1UL, 0UL, {"Error"}}, "simple_error.node", aedis::error::resp3_simple_error};
   auto const in02 = expect<node_type>{"-\r\n", node_type{resp3::type::simple_error, 1UL, 0UL, {""}}, "simple_error.node.empty", aedis::error::resp3_simple_error};

   auto ex = ioc.get_executor();

   test_sync(ex, in01);
   test_sync(ex, in02);

   test_async(ex, in01);
   test_async(ex, in02);
   ioc.run();
}

BOOST_AUTO_TEST_CASE(test_blob_string)
{
   net::io_context ioc;
   std::string str(100000, 'a');
   str[1000] = '\r';
   str[1001] = '\n';

   std::string wire;
   wire += '$';
   wire += std::to_string(str.size());
   wire += "\r\n";
   wire += str;
   wire += "\r\n";

   auto const in01 = expect<node_type>{"$2\r\nhh\r\n", node_type{resp3::type::blob_string, 1UL, 0UL, {"hh"}}, "blob_string.node"};
   auto const in02 = expect<node_type>{"$26\r\nhhaa\aaaa\raaaaa\r\naaaaaaaaaa\r\n", node_type{resp3::type::blob_string, 1UL, 0UL, {"hhaa\aaaa\raaaaa\r\naaaaaaaaaa"}}, "blob_string.node (with separator)"};
   auto const in03 = expect<node_type>{"$0\r\n\r\n", node_type{resp3::type::blob_string, 1UL, 0UL, {}}, "blob_string.node.empty"};
   auto const in04 = expect<node_type>{wire, node_type{resp3::type::blob_string, 1UL, 0UL, {str}}, "blob_string.node (long string)"};

   auto ex = ioc.get_executor();

   test_sync(ex, in01);
   test_sync(ex, in02);
   test_sync(ex, in03);
   test_sync(ex, in04);

   test_async(ex, in01);
   test_async(ex, in02);
   test_async(ex, in03);
   test_async(ex, in04);
   ioc.run();
}

BOOST_AUTO_TEST_CASE(test_double)
{
   net::io_context ioc;
   auto const in01 = expect<node_type>{",1.23\r\n", node_type{resp3::type::doublean, 1UL, 0UL, {"1.23"}}, "double.node"};
   auto const in02 = expect<node_type>{",inf\r\n", node_type{resp3::type::doublean, 1UL, 0UL, {"inf"}}, "double.node (inf)"};
   auto const in03 = expect<node_type>{",-inf\r\n", node_type{resp3::type::doublean, 1UL, 0UL, {"-inf"}}, "double.node (-inf)"};
   auto const in04 = expect<double>{",1.23\r\n", double{1.23}, "double.double"};
   auto const in05 = expect<double>{",er\r\n", double{0}, "double.double", aedis::error::not_a_double};

   auto ex = ioc.get_executor();

   test_sync(ex, in01);
   test_sync(ex, in02);
   test_sync(ex, in03);
   test_sync(ex, in04);
   test_sync(ex, in05);

   test_async(ex, in01);
   test_async(ex, in02);
   test_async(ex, in03);
   test_async(ex, in04);
   test_async(ex, in05);
   ioc.run();
}

BOOST_AUTO_TEST_CASE(test_blob_error)
{
   net::io_context ioc;
   auto const in01 = expect<node_type>{"!21\r\nSYNTAX invalid syntax\r\n", node_type{resp3::type::blob_error, 1UL, 0UL, {"SYNTAX invalid syntax"}}, "blob_error", aedis::error::resp3_blob_error};
   auto const in02 = expect<node_type>{"!0\r\n\r\n", node_type{resp3::type::blob_error, 1UL, 0UL, {}}, "blob_error.empty", aedis::error::resp3_blob_error};

   auto ex = ioc.get_executor();

   test_sync(ex, in01);
   test_sync(ex, in02);

   test_async(ex, in01);
   test_async(ex, in02);
   ioc.run();
}

BOOST_AUTO_TEST_CASE(test_verbatim_string)
{
   net::io_context ioc;
   auto const in01 = expect<node_type>{"=15\r\ntxt:Some string\r\n", node_type{resp3::type::verbatim_string, 1UL, 0UL, {"txt:Some string"}}, "verbatim_string"};
   auto const in02 = expect<node_type>{"=0\r\n\r\n", node_type{resp3::type::verbatim_string, 1UL, 0UL, {}}, "verbatim_string.empty"};

   auto ex = ioc.get_executor();

   test_sync(ex, in01);
   test_sync(ex, in02);

   test_async(ex, in01);
   test_async(ex, in02);
   ioc.run();
}

BOOST_AUTO_TEST_CASE(test_big_number)
{
   net::io_context ioc;
   auto const in01 = expect<node_type>{"(3492890328409238509324850943850943825024385\r\n", node_type{resp3::type::big_number, 1UL, 0UL, {"3492890328409238509324850943850943825024385"}}, "big_number.node"};
   auto const in02 = expect<int>{"(\r\n", int{}, "big_number.error (empty field)", aedis::error::empty_field};

   auto ex = ioc.get_executor();

   test_sync(ex, in01);
   test_sync(ex, in02);

   test_async(ex, in01);
   test_async(ex, in02);
   ioc.run();
}

BOOST_AUTO_TEST_CASE(test_simple_string)
{
   net::io_context ioc;
   boost::optional<std::string> ok1, ok2;
   ok1 = "OK";
   ok2 = "";

   auto in00 = expect<std::string>{"+OK\r\n", std::string{"OK"}, "simple_string.string"};
   auto in01 = expect<std::string>{"+\r\n", std::string{""}, "simple_string.string.empty"};
   auto in02 = expect<boost::optional<std::string>>{"+OK\r\n", boost::optional<std::string>{"OK"}, "simple_string.optional"};
   auto in03 = expect<boost::optional<std::string>>{"+\r\n", boost::optional<std::string>{""}, "simple_string.optional.empty"};

   auto ex = ioc.get_executor();

   test_sync(ex, in00);
   test_sync(ex, in01);
   test_sync(ex, in02);
   test_sync(ex, in03);

   test_async(ex, in00);
   test_async(ex, in01);
   test_async(ex, in02);
   test_async(ex, in03);
   ioc.run();
}

BOOST_AUTO_TEST_CASE(test_resp3)
{
   using map_type = std::map<std::string, std::string>;
   std::string const wire_map = "%rt\r\n$4\r\nkey1\r\n$6\r\nvalue1\r\n$4\r\nkey2\r\n$6\r\nvalue2\r\n";

   std::string const wire_ssp = "$?\r\n;d\r\nHell\r\n;5\r\no wor\r\n;1\r\nd\r\n;0\r\n";

   net::io_context ioc;
   auto const in01 = expect<int>{"s11\r\n", int{}, "number.error", aedis::error::invalid_data_type};
   auto const in02 = expect<int>{":adf\r\n", int{11}, "number.int", aedis::error::not_a_number};
   auto const in03 = expect<map_type>{wire_map, map_type{}, "map.error", aedis::error::not_a_number};
   auto const in04 = expect<std::string>{wire_ssp, std::string{}, "streamed_string_part.error", aedis::error::not_a_number};
   auto const in05 = expect<std::string>{"$l\r\nhh\r\n", std::string{}, "blob_string.error", aedis::error::not_a_number};
   auto const in06 = expect<int>{":\r\n", int{}, "number.error (empty field)", aedis::error::empty_field};
   auto const in07 = expect<boost::optional<bool>>{"#\r\n", boost::optional<bool>{}, "bool.error", aedis::error::empty_field};
   auto const in08 = expect<std::string>{",\r\n", std::string{}, "double.error (empty field)", aedis::error::empty_field};

   auto ex = ioc.get_executor();

   test_sync(ex, in01);
   test_sync(ex, in02);
   test_sync(ex, in03);
   test_sync(ex, in04);
   test_sync(ex, in05);
   test_sync(ex, in06);
   test_sync(ex, in07);
   test_sync(ex, in08);

   test_async(ex, in01);
   test_async(ex, in02);
   test_async(ex, in03);
   test_async(ex, in04);
   test_async(ex, in05);
   test_async(ex, in06);
   test_async(ex, in07);
   test_async(ex, in08);

   ioc.run();
}

BOOST_AUTO_TEST_CASE(test_null)
{
   net::io_context ioc;
   using op_type_01 = boost::optional<bool>;
   using op_type_02 = boost::optional<int>;
   using op_type_03 = boost::optional<std::string>;
   using op_type_04 = boost::optional<std::vector<std::string>>;
   using op_type_05 = boost::optional<std::list<std::string>>;
   using op_type_06 = boost::optional<std::map<std::string, std::string>>;
   using op_type_07 = boost::optional<std::unordered_map<std::string, std::string>>;
   using op_type_08 = boost::optional<std::set<std::string>>;
   using op_type_09 = boost::optional<std::unordered_set<std::string>>;

   auto const in01 = expect<op_type_01>{"_\r\n", op_type_01{}, "null.optional.bool"};
   auto const in02 = expect<op_type_02>{"_\r\n", op_type_02{}, "null.optional.int"};
   auto const in03 = expect<op_type_03>{"_\r\n", op_type_03{}, "null.optional.string"};
   auto const in04 = expect<op_type_04>{"_\r\n", op_type_04{}, "null.optional.vector"};
   auto const in05 = expect<op_type_05>{"_\r\n", op_type_05{}, "null.optional.list"};
   auto const in06 = expect<op_type_06>{"_\r\n", op_type_06{}, "null.optional.map"};
   auto const in07 = expect<op_type_07>{"_\r\n", op_type_07{}, "null.optional.unordered_map"};
   auto const in08 = expect<op_type_08>{"_\r\n", op_type_08{}, "null.optional.set"};
   auto const in09 = expect<op_type_09>{"_\r\n", op_type_09{}, "null.optional.unordered_set"};

   auto ex = ioc.get_executor();

   test_sync(ex, in01);
   test_sync(ex, in02);
   test_sync(ex, in03);
   test_sync(ex, in04);
   test_sync(ex, in05);
   test_sync(ex, in06);
   test_sync(ex, in07);
   test_sync(ex, in08);
   test_sync(ex, in09);

   test_async(ex, in01);
   test_async(ex, in02);
   test_async(ex, in03);
   test_async(ex, in04);
   test_async(ex, in05);
   test_async(ex, in06);
   test_async(ex, in07);
   test_async(ex, in08);
   test_async(ex, in09);
   ioc.run();
}

BOOST_AUTO_TEST_CASE(all_tests)
{
   net::io_context ioc;

   auto ex = ioc.get_executor();

#define TEST test_sync
   NUMBER_TEST_CONDITIONS(TEST)
#undef TEST

#define TEST test_async
   NUMBER_TEST_CONDITIONS(TEST)
#undef TEST

   ioc.run();
}

//-----------------------------------------------------------------------------------
void check_error(char const* name, aedis::error ev)
{
   auto const ec = aedis::make_error_code(ev);
   auto const& cat = ec.category();
   BOOST_TEST(std::string(ec.category().name()) == name);
   BOOST_TEST(!ec.message().empty());
   BOOST_TEST(cat.equivalent(
      static_cast<std::underlying_type<aedis::error>::type>(ev),
          ec.category().default_error_condition(
              static_cast<std::underlying_type<aedis::error>::type>(ev))));
   BOOST_TEST(cat.equivalent(ec,
      static_cast<std::underlying_type<aedis::error>::type>(ev)));
}

BOOST_AUTO_TEST_CASE(error)
{
   check_error("aedis", aedis::error::resolve_timeout);
   check_error("aedis", aedis::error::resolve_timeout);
   check_error("aedis", aedis::error::connect_timeout);
   check_error("aedis", aedis::error::idle_timeout);
   check_error("aedis", aedis::error::exec_timeout);
   check_error("aedis", aedis::error::invalid_data_type);
   check_error("aedis", aedis::error::not_a_number);
   check_error("aedis", aedis::error::exceeeds_max_nested_depth);
   check_error("aedis", aedis::error::unexpected_bool_value);
   check_error("aedis", aedis::error::empty_field);
   check_error("aedis", aedis::error::expects_resp3_simple_type);
   check_error("aedis", aedis::error::expects_resp3_aggregate);
   check_error("aedis", aedis::error::expects_resp3_map);
   check_error("aedis", aedis::error::expects_resp3_set);
   check_error("aedis", aedis::error::nested_aggregate_not_supported);
   check_error("aedis", aedis::error::resp3_simple_error);
   check_error("aedis", aedis::error::resp3_blob_error);
   check_error("aedis", aedis::error::incompatible_size);
   check_error("aedis", aedis::error::not_a_double);
   check_error("aedis", aedis::error::resp3_null);
}

std::string get_type_as_str(aedis::resp3::type t)
{
   std::ostringstream ss;
   ss << t;
   return ss.str();
}

BOOST_AUTO_TEST_CASE(type_string)
{
   BOOST_TEST(!get_type_as_str(aedis::resp3::type::array).empty());
   BOOST_TEST(!get_type_as_str(aedis::resp3::type::push).empty());
   BOOST_TEST(!get_type_as_str(aedis::resp3::type::set).empty());
   BOOST_TEST(!get_type_as_str(aedis::resp3::type::map).empty());
   BOOST_TEST(!get_type_as_str(aedis::resp3::type::attribute).empty());
   BOOST_TEST(!get_type_as_str(aedis::resp3::type::simple_string).empty());
   BOOST_TEST(!get_type_as_str(aedis::resp3::type::simple_error).empty());
   BOOST_TEST(!get_type_as_str(aedis::resp3::type::number).empty());
   BOOST_TEST(!get_type_as_str(aedis::resp3::type::doublean).empty());
   BOOST_TEST(!get_type_as_str(aedis::resp3::type::boolean).empty());
   BOOST_TEST(!get_type_as_str(aedis::resp3::type::big_number).empty());
   BOOST_TEST(!get_type_as_str(aedis::resp3::type::null).empty());
   BOOST_TEST(!get_type_as_str(aedis::resp3::type::blob_error).empty());
   BOOST_TEST(!get_type_as_str(aedis::resp3::type::verbatim_string).empty());
   BOOST_TEST(!get_type_as_str(aedis::resp3::type::blob_string).empty());
   BOOST_TEST(!get_type_as_str(aedis::resp3::type::streamed_string_part).empty());
   BOOST_TEST(!get_type_as_str(aedis::resp3::type::invalid).empty());
}

BOOST_AUTO_TEST_CASE(type_convert)
{
   using aedis::resp3::to_code;
   using aedis::resp3::to_type;
   using aedis::resp3::type;

#define CHECK_CASE(A) BOOST_CHECK_EQUAL(to_type(to_code(type::A)), type::A);
   CHECK_CASE(array);
   CHECK_CASE(push);
   CHECK_CASE(set);
   CHECK_CASE(map);
   CHECK_CASE(attribute);
   CHECK_CASE(simple_string);
   CHECK_CASE(simple_error);
   CHECK_CASE(number);
   CHECK_CASE(doublean);
   CHECK_CASE(boolean);
   CHECK_CASE(big_number);
   CHECK_CASE(null);
   CHECK_CASE(blob_error);
   CHECK_CASE(verbatim_string);
   CHECK_CASE(blob_string);
   CHECK_CASE(streamed_string_part);
#undef CHECK_CASE
}

