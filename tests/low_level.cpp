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

namespace std
{
auto operator==(aedis::ignore, aedis::ignore) noexcept {return true;}
auto operator!=(aedis::ignore, aedis::ignore) noexcept {return false;}
}

namespace net = boost::asio;
namespace resp3 = aedis::resp3;

using test_stream = boost::beast::test::stream;
using aedis::adapter::adapt2;
using node_type = aedis::resp3::node<std::string>;
using vec_node_type = std::vector<node_type>;
using vec_type = std::vector<std::string>;
using op_vec_type = std::optional<std::vector<std::string>>;

// Set
using set_type = std::set<std::string>;
using mset_type = std::multiset<std::string>;
using uset_type = std::unordered_set<std::string>;
using muset_type = std::unordered_multiset<std::string>;

// Array
using tuple_int_2 = std::tuple<int, int>;
using array_type = std::array<int, 3>;
using array_type2 = std::array<int, 1>;

// Map
using map_type = std::map<std::string, std::string>;
using mmap_type = std::multimap<std::string, std::string>;
using umap_type = std::unordered_map<std::string, std::string>;
using mumap_type = std::unordered_multimap<std::string, std::string>;
using op_map_type = std::optional<std::map<std::string, std::string>>;
using tuple8_type = std::tuple<std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string>;

// Null
using op_type_01 = std::optional<bool>;
using op_type_02 = std::optional<int>;
using op_type_03 = std::optional<std::string>;
using op_type_04 = std::optional<std::vector<std::string>>;
using op_type_05 = std::optional<std::list<std::string>>;
using op_type_06 = std::optional<std::map<std::string, std::string>>;
using op_type_07 = std::optional<std::unordered_map<std::string, std::string>>;
using op_type_08 = std::optional<std::set<std::string>>;
using op_type_09 = std::optional<std::unordered_set<std::string>>;

//-------------------------------------------------------------------

template <class Result>
struct expect {
   std::string in;
   Result expected;
   boost::system::error_code ec{};
};

template <class Result>
auto make_expected(std::string in, Result expected, boost::system::error_code ec = {})
{
   return expect<Result>{in, expected, ec};
}

template <class Result>
void test_sync(net::any_io_executor ex, expect<Result> e)
{
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

auto make_blob()
{
   std::string str(100000, 'a');
   str[1000] = '\r';
   str[1001] = '\n';
   return str;

   return str;
}

auto const blob = make_blob();

auto make_blob_string(std::string const& b)
{
   std::string wire;
   wire += '$';
   wire += std::to_string(b.size());
   wire += "\r\n";
   wire += b;
   wire += "\r\n";

   return wire;
}

std::optional<int> op_int_ok = 11;
std::optional<bool> op_bool_ok = true;

// TODO: Test a streamed string that is not finished with a string of
// size 0 but other command comes in.
std::vector<node_type> streamed_string_e1
{ {aedis::resp3::type::streamed_string, 0, 1, ""}
, {aedis::resp3::type::streamed_string_part, 1, 1, "Hell"}
, {aedis::resp3::type::streamed_string_part, 1, 1, "o wor"}
, {aedis::resp3::type::streamed_string_part, 1, 1, "d"}
, {aedis::resp3::type::streamed_string_part, 1, 1, ""}
};

std::vector<node_type> streamed_string_e2 { {resp3::type::streamed_string, 0UL, 1UL, {}}, {resp3::type::streamed_string_part, 1UL, 1UL, {}} };

std::vector<node_type> const push_e1a
   { {resp3::type::push,          4UL, 0UL, {}}
   , {resp3::type::simple_string, 1UL, 1UL, "pubsub"}
   , {resp3::type::simple_string, 1UL, 1UL, "message"}
   , {resp3::type::simple_string, 1UL, 1UL, "some-channel"}
   , {resp3::type::simple_string, 1UL, 1UL, "some message"}
   };

std::vector<node_type> const push_e1b
   { {resp3::type::push, 0UL, 0UL, {}} };

std::vector<node_type> const set_expected1a
   { {resp3::type::set,            6UL, 0UL, {}}
   , {resp3::type::simple_string,  1UL, 1UL, {"orange"}}
   , {resp3::type::simple_string,  1UL, 1UL, {"apple"}}
   , {resp3::type::simple_string,  1UL, 1UL, {"one"}}
   , {resp3::type::simple_string,  1UL, 1UL, {"two"}}
   , {resp3::type::simple_string,  1UL, 1UL, {"three"}}
   , {resp3::type::simple_string,  1UL, 1UL, {"orange"}}
   };

mset_type const set_e1f{"apple", "one", "orange", "orange", "three", "two"};
uset_type const set_e1c{"apple", "one", "orange", "three", "two"};
muset_type const set_e1g{"apple", "one", "orange", "orange", "three", "two"};
vec_type const set_e1d = {"orange", "apple", "one", "two", "three", "orange"};
op_vec_type const set_expected_1e = set_e1d;

std::vector<node_type> const array_e1a
   { {resp3::type::array,       3UL, 0UL, {}}
   , {resp3::type::blob_string, 1UL, 1UL, {"11"}}
   , {resp3::type::blob_string, 1UL, 1UL, {"22"}}
   , {resp3::type::blob_string, 1UL, 1UL, {"3"}}
   };

std::vector<int> const array_e1b{11, 22, 3};
std::vector<std::string> const array_e1c{"11", "22", "3"};
std::vector<std::string> const array_e1d{};
std::vector<node_type> const array_e1e{{resp3::type::array, 0UL, 0UL, {}}};
array_type const array_e1f{11, 22, 3};
std::list<int> const array_e1g{11, 22, 3};
std::deque<int> const array_e1h{11, 22, 3};

std::vector<node_type> const map_expected_1a
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

map_type const map_expected_1b
{ {"key1", "value1"}
, {"key2", "value2"}
, {"key3", "value3"}
};

umap_type const map_e1g
{ {"key1", "value1"}
, {"key2", "value2"}
, {"key3", "value3"}
};

mmap_type const map_e1k
{ {"key1", "value1"}
, {"key2", "value2"}
, {"key3", "value3"}
, {"key3", "value3"}
};

mumap_type const map_e1l
{ {"key1", "value1"}
, {"key2", "value2"}
, {"key3", "value3"}
, {"key3", "value3"}
};

std::vector<std::string> const map_expected_1c
{ "key1", "value1"
, "key2", "value2"
, "key3", "value3"
, "key3", "value3"
};

op_map_type const map_expected_1d = map_expected_1b;

op_vec_type const map_expected_1e = map_expected_1c;

tuple8_type const map_e1f
{ std::string{"key1"}, std::string{"value1"}
, std::string{"key2"}, std::string{"value2"}
, std::string{"key3"}, std::string{"value3"}
, std::string{"key3"}, std::string{"value3"}
};

std::vector<node_type> const attr_e1a
   { {resp3::type::attribute,     1UL, 0UL, {}}
   , {resp3::type::simple_string, 1UL, 1UL, "key-popularity"}
   , {resp3::type::map,           2UL, 1UL, {}}
   , {resp3::type::blob_string,   1UL, 2UL, "a"}
   , {resp3::type::doublean,      1UL, 2UL, "0.1923"}
   , {resp3::type::blob_string,   1UL, 2UL, "b"}
   , {resp3::type::doublean,      1UL, 2UL, "0.0012"}
   };

std::vector<node_type> const attr_e1b
   { {resp3::type::attribute, 0UL, 0UL, {}} };

#define S01 "#11\r\n"
#define S02 "#f\r\n"
#define S03 "#t\r\n"
#define S04 "$?\r\n;0\r\n"
#define S05 "%11\r\n"
#define S06 "$?\r\n;4\r\nHell\r\n;5\r\no wor\r\n;1\r\nd\r\n;0\r\n"
#define S07 "$?\r\n;b\r\nHell\r\n;5\r\no wor\r\n;1\r\nd\r\n;0\r\n"
#define S08 "*1\r\n:11\r\n"
#define S09 ":-3\r\n"
#define S10 ":11\r\n"
#define S11 ":3\r\n"
#define S12 "_\r\n"
#define S13 ">4\r\n+pubsub\r\n+message\r\n+some-channel\r\n+some message\r\n"
#define S14 ">0\r\n"
#define S15 "*3\r\n$2\r\n11\r\n$2\r\n22\r\n$1\r\n3\r\n"
#define S16 "%4\r\n$4\r\nkey1\r\n$6\r\nvalue1\r\n$4\r\nkey2\r\n$6\r\nvalue2\r\n$4\r\nkey3\r\n$6\r\nvalue3\r\n$4\r\nkey3\r\n$6\r\nvalue3\r\n"
#define S17 "*1\r\n" S16
#define S18 "|1\r\n+key-popularity\r\n%2\r\n$1\r\na\r\n,0.1923\r\n$1\r\nb\r\n,0.0012\r\n"
#define S19 "|0\r\n"
#define S20 "*3\r\n$2\r\n11\r\n$2\r\n22\r\n$1\r\n3\r\n"
#define S21 "*1\r\n*1\r\n$2\r\nab\r\n"
#define S22 "*1\r\n*1\r\n*1\r\n*1\r\n*1\r\n*1\r\na\r\n"
#define S23 "*0\r\n"
#define S24 "*3\r\n$2\r\n11\r\n$2\r\n22\r\n$1\r\n3\r\n"
#define S25 "~6\r\n+orange\r\n+apple\r\n+one\r\n+two\r\n+three\r\n+orange\r\n"
#define S26 "*1\r\n" S25
#define S27 "~0\r\n"
#define S28 "-Error\r\n"
#define S29 "-\r\n"
#define S30 "%0\r\n"
#define S31 ",1.23\r\n"
#define S32 ",inf\r\n"
#define S33 ",-inf\r\n" 
#define S34 ",1.23\r\n"
#define S35 ",er\r\n"
#define S36 "!21\r\nSYNTAX invalid syntax\r\n"
#define S37 "!0\r\n\r\n"
#define S38 "!3\r\nfoo\r\n"
#define S39 "=15\r\ntxt:Some string\r\n"
#define S40 "=0\r\n\r\n"
#define S41 "(3492890328409238509324850943850943825024385\r\n"
#define S42 "(\r\n"
#define S43 "+OK\r\n"
#define S44 "+\r\n"
#define S45 "s11\r\n"
#define S46 ":adf\r\n"
#define S47 "%rt\r\n$4\r\nkey1\r\n$6\r\nvalue1\r\n$4\r\nkey2\r\n$6\r\nvalue2\r\n"
#define S48 "$?\r\n;d\r\nHell\r\n;5\r\no wor\r\n;1\r\nd\r\n;0\r\n"
#define S49 "$l\r\nhh\r\n"
#define S50 ":\r\n"
#define S51 "#\r\n"
#define S52 ",\r\n"
#define S53 "$2\r\nhh\r\n"
#define S54 "$26\r\nhhaa\aaaa\raaaaa\r\naaaaaaaaaa\r\n"
#define S55 "$0\r\n\r\n" 

#define NUMBER_TEST_CONDITIONS(test) \
   test(ex, make_expected(S01, std::optional<bool>{}, aedis::error::unexpected_bool_value)); \
   test(ex, make_expected(S02, bool{false})); \
   test(ex, make_expected(S02, node_type{resp3::type::boolean, 1UL, 0UL, {"f"}})); \
   test(ex, make_expected(S03, bool{true})); \
   test(ex, make_expected(S03, node_type{resp3::type::boolean, 1UL, 0UL, {"t"}})); \
   test(ex, make_expected(S03, op_bool_ok)); \
   test(ex, make_expected(S03, std::map<int, int>{}, aedis::error::expects_resp3_map)); \
   test(ex, make_expected(S03, std::set<int>{}, aedis::error::expects_resp3_set)); \
   test(ex, make_expected(S03, std::unordered_map<int, int>{}, aedis::error::expects_resp3_map)); \
   test(ex, make_expected(S03, std::unordered_set<int>{}, aedis::error::expects_resp3_set)); \
   test(ex, make_expected(S04, streamed_string_e2)); \
   test(ex, make_expected(S05, int{}, aedis::error::expects_resp3_simple_type));\
   test(ex, make_expected(S05, std::optional<int>{}, aedis::error::expects_resp3_simple_type));; \
   test(ex, make_expected(S06, int{}, aedis::error::not_a_number)); \
   test(ex, make_expected(S06, std::string{"Hello word"})); \
   test(ex, make_expected(S06, streamed_string_e1)); \
   test(ex, make_expected(S07, std::string{}, aedis::error::not_a_number)); \
   test(ex, make_expected(S08, std::tuple<int>{11})); \
   test(ex, make_expected(S09, node_type{resp3::type::number, 1UL, 0UL, {"-3"}})); \
   test(ex, make_expected(S10, int{11})); \
   test(ex, make_expected(S10, op_int_ok)); \
   test(ex, make_expected(S10, std::list<std::string>{}, aedis::error::expects_resp3_aggregate)); \
   test(ex, make_expected(S10, std::map<std::string, std::string>{}, aedis::error::expects_resp3_map)); \
   test(ex, make_expected(S10, std::set<std::string>{}, aedis::error::expects_resp3_set)); \
   test(ex, make_expected(S10, std::unordered_map<std::string, std::string>{}, aedis::error::expects_resp3_map)); \
   test(ex, make_expected(S10, std::unordered_set<std::string>{}, aedis::error::expects_resp3_set)); \
   test(ex, make_expected(S11, array_type2{}, aedis::error::expects_resp3_aggregate));\
   test(ex, make_expected(S11, node_type{resp3::type::number, 1UL, 0UL, {"3"}})); \
   test(ex, make_expected(S12, array_type{}, aedis::error::resp3_null));\
   test(ex, make_expected(S12, int{0}, aedis::error::resp3_null)); \
   test(ex, make_expected(S12, map_type{}, aedis::error::resp3_null));\
   test(ex, make_expected(S12, op_type_01{}));\
   test(ex, make_expected(S12, op_type_02{}));\
   test(ex, make_expected(S12, op_type_03{}));\
   test(ex, make_expected(S12, op_type_04{}));\
   test(ex, make_expected(S12, op_type_05{}));\
   test(ex, make_expected(S12, op_type_06{}));\
   test(ex, make_expected(S12, op_type_07{}));\
   test(ex, make_expected(S12, op_type_08{}));\
   test(ex, make_expected(S12, op_type_09{}));\
   test(ex, make_expected(S12, std::list<int>{}, aedis::error::resp3_null));\
   test(ex, make_expected(S12, std::vector<int>{}, aedis::error::resp3_null));\
   test(ex, make_expected(S13, push_e1a)); \
   test(ex, make_expected(S14, push_e1b)); \
   test(ex, make_expected(S15, map_type{}, aedis::error::expects_resp3_map));\
   test(ex, make_expected(S16, map_e1f));\
   test(ex, make_expected(S16, map_e1g));\
   test(ex, make_expected(S16, map_e1k));\
   test(ex, make_expected(S16, map_e1l));\
   test(ex, make_expected(S16, map_expected_1a));\
   test(ex, make_expected(S16, map_expected_1b));\
   test(ex, make_expected(S16, map_expected_1c));\
   test(ex, make_expected(S16, map_expected_1d));\
   test(ex, make_expected(S16, map_expected_1e));\
   test(ex, make_expected(S17, std::tuple<op_map_type>{map_expected_1d}));\
   test(ex, make_expected(S18, attr_e1a)); \
   test(ex, make_expected(S19, attr_e1b)); \
   test(ex, make_expected(S20, array_e1a));\
   test(ex, make_expected(S20, array_e1b));\
   test(ex, make_expected(S20, array_e1c));\
   test(ex, make_expected(S20, array_e1f));\
   test(ex, make_expected(S20, array_e1g));\
   test(ex, make_expected(S20, array_e1h));\
   test(ex, make_expected(S20, array_type2{}, aedis::error::incompatible_size));\
   test(ex, make_expected(S20, tuple_int_2{}, aedis::error::incompatible_size));\
   test(ex, make_expected(S21, array_type2{}, aedis::error::nested_aggregate_not_supported));\
   test(ex, make_expected(S22, vec_node_type{}, aedis::error::exceeeds_max_nested_depth));\
   test(ex, make_expected(S23, array_e1d));\
   test(ex, make_expected(S23, array_e1e));\
   test(ex, make_expected(S24, set_type{}, aedis::error::expects_resp3_set)); \
   test(ex, make_expected(S25, set_e1c)); \
   test(ex, make_expected(S25, set_e1d)); \
   test(ex, make_expected(S25, set_e1f)); \
   test(ex, make_expected(S25, set_e1g)); \
   test(ex, make_expected(S25, set_expected1a)); \
   test(ex, make_expected(S25, set_expected_1e)); \
   test(ex, make_expected(S25, set_type{"apple", "one", "orange", "three", "two"})); \
   test(ex, make_expected(S26, std::tuple<uset_type>{set_e1c})); \
   test(ex, make_expected(S27, std::vector<node_type>{ {resp3::type::set,  0UL, 0UL, {}} })); \
   test(ex, make_expected(S28, aedis::ignore{}, aedis::error::resp3_simple_error)); \
   test(ex, make_expected(S28, node_type{resp3::type::simple_error, 1UL, 0UL, {"Error"}}, aedis::error::resp3_simple_error)); \
   test(ex, make_expected(S29, node_type{resp3::type::simple_error, 1UL, 0UL, {""}}, aedis::error::resp3_simple_error)); \
   test(ex, make_expected(S30, map_type{}));\
   test(ex, make_expected(S31, node_type{resp3::type::doublean, 1UL, 0UL, {"1.23"}}));\
   test(ex, make_expected(S32, node_type{resp3::type::doublean, 1UL, 0UL, {"inf"}}));\
   test(ex, make_expected(S33, node_type{resp3::type::doublean, 1UL, 0UL, {"-inf"}}));\
   test(ex, make_expected(S34, double{1.23}));\
   test(ex, make_expected(S35, double{0}, aedis::error::not_a_double));\
   test(ex, make_expected(S36, node_type{resp3::type::blob_error, 1UL, 0UL, {"SYNTAX invalid syntax"}}, aedis::error::resp3_blob_error));\
   test(ex, make_expected(S37, node_type{resp3::type::blob_error, 1UL, 0UL, {}}, aedis::error::resp3_blob_error));\
   test(ex, make_expected(S38, aedis::ignore{}, aedis::error::resp3_blob_error));\
   test(ex, make_expected(S39, node_type{resp3::type::verbatim_string, 1UL, 0UL, {"txt:Some string"}}));\
   test(ex, make_expected(S40, node_type{resp3::type::verbatim_string, 1UL, 0UL, {}}));\
   test(ex, make_expected(S41, node_type{resp3::type::big_number, 1UL, 0UL, {"3492890328409238509324850943850943825024385"}}));\
   test(ex, make_expected(S42, int{}, aedis::error::empty_field));\
   test(ex, make_expected(S43, std::optional<std::string>{"OK"}));\
   test(ex, make_expected(S43, std::string{"OK"}));\
   test(ex, make_expected(S44, std::optional<std::string>{""}));\
   test(ex, make_expected(S44, std::string{""}));\
   test(ex, make_expected(S45, int{}, aedis::error::invalid_data_type));\
   test(ex, make_expected(S46, int{11}, aedis::error::not_a_number));\
   test(ex, make_expected(S47, map_type{}, aedis::error::not_a_number));\
   test(ex, make_expected(S48, std::string{}, aedis::error::not_a_number));\
   test(ex, make_expected(S49, std::string{}, aedis::error::not_a_number));\
   test(ex, make_expected(S50, int{}, aedis::error::empty_field));\
   test(ex, make_expected(S51, std::optional<bool>{}, aedis::error::empty_field));\
   test(ex, make_expected(S52, std::string{}, aedis::error::empty_field));\
   test(ex, make_expected(S53, node_type{resp3::type::blob_string, 1UL, 0UL, {"hh"}}));\
   test(ex, make_expected(S54, node_type{resp3::type::blob_string, 1UL, 0UL, {"hhaa\aaaa\raaaaa\r\naaaaaaaaaa"}}));\
   test(ex, make_expected(S55, node_type{resp3::type::blob_string, 1UL, 0UL, {}}));\
   test(ex, make_expected(make_blob_string(blob), node_type{resp3::type::blob_string, 1UL, 0UL, {blob}}));\

BOOST_AUTO_TEST_CASE(parser)
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

BOOST_AUTO_TEST_CASE(ignore_adapter_simple_error)
{
   net::io_context ioc;
   std::string rbuffer;

   boost::system::error_code ec;

   test_stream ts {ioc};
   ts.append(S28);
   resp3::read(ts, net::dynamic_buffer(rbuffer), adapt2(), ec);
   BOOST_CHECK_EQUAL(ec, aedis::error::resp3_simple_error);
   BOOST_TEST(!rbuffer.empty());
}

BOOST_AUTO_TEST_CASE(ignore_adapter_blob_error)
{
   net::io_context ioc;
   std::string rbuffer;
   boost::system::error_code ec;

   test_stream ts {ioc};
   ts.append(S36);
   resp3::read(ts, net::dynamic_buffer(rbuffer), adapt2(), ec);
   BOOST_CHECK_EQUAL(ec, aedis::error::resp3_blob_error);
   BOOST_TEST(!rbuffer.empty());
}

BOOST_AUTO_TEST_CASE(ignore_adapter_no_error)
{
   net::io_context ioc;
   std::string rbuffer;
   boost::system::error_code ec;

   test_stream ts {ioc};
   ts.append(S10);
   resp3::read(ts, net::dynamic_buffer(rbuffer), adapt2(), ec);
   BOOST_TEST(!ec);
   BOOST_TEST(rbuffer.empty());
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
   check_error("aedis", aedis::error::not_connected);
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

BOOST_AUTO_TEST_CASE(adapter)
{
   using aedis::adapt;
   using resp3::type;

   boost::system::error_code ec;

   std::string a;
   int b;
   auto resp = std::tie(a, b, std::ignore);

   auto f = adapt(resp);
   f(0, resp3::node<std::string_view>{type::simple_string, 1, 0, "Hello"}, ec);
   f(1, resp3::node<std::string_view>{type::number, 1, 0, "42"}, ec);

   BOOST_CHECK_EQUAL(a, "Hello");
   BOOST_TEST(!ec);

   BOOST_CHECK_EQUAL(b, 42);
   BOOST_TEST(!ec);
}

