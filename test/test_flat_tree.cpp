/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/adapter/adapt.hpp>
#include <boost/redis/resp3/flat_tree.hpp>

#include <boost/core/lightweight_test.hpp>

#include "print_node.hpp"

using boost::redis::adapter::adapt2;
using boost::redis::adapter::result;
using boost::redis::resp3::tree;
using boost::redis::resp3::flat_tree;
using boost::redis::generic_flat_response;
using boost::redis::ignore_t;
using boost::redis::resp3::detail::deserialize;
using boost::redis::resp3::node;
using boost::redis::resp3::node_view;
using boost::redis::resp3::to_string;
using boost::redis::response;
using boost::system::error_code;

namespace {

#define RESP3_SET_PART1 "~6\r\n+orange\r"
#define RESP3_SET_PART2 "\n+apple\r\n+one"
#define RESP3_SET_PART3 "\r\n+two\r"
#define RESP3_SET_PART4 "\n+three\r\n+orange\r\n"
char const* resp3_set = RESP3_SET_PART1 RESP3_SET_PART2 RESP3_SET_PART3 RESP3_SET_PART4;

node from_node_view(node_view const& v)
{
   node ret;
   ret.data_type = v.data_type;
   ret.aggregate_size = v.aggregate_size;
   ret.depth = v.depth;
   ret.value = v.value;
   return ret;
}

tree from_flat(generic_flat_response const& resp)
{
   tree ret;
   for (auto const& e : resp.value().get_view())
      ret.push_back(from_node_view(e));

   return ret;
}

tree from_flat(flat_tree const& resp)
{
   tree ret;
   for (auto const& e : resp.get_view())
      ret.push_back(from_node_view(e));

   return ret;
}

// Parses the same data into a tree and a
// flat_tree, they should be equal to each other.
void test_views_are_set()
{
   tree resp1;
   flat_tree resp2;
   generic_flat_response resp3;

   error_code ec;
   deserialize(resp3_set, adapt2(resp1), ec);
   BOOST_TEST_EQ(ec, error_code{});

   deserialize(resp3_set, adapt2(resp2), ec);
   BOOST_TEST_EQ(ec, error_code{});

   deserialize(resp3_set, adapt2(resp3), ec);
   BOOST_TEST_EQ(ec, error_code{});

   BOOST_TEST_EQ(resp2.get_reallocs(), 1u);
   BOOST_TEST_EQ(resp2.get_total_msgs(), 1u);

   BOOST_TEST_EQ(resp3.value().get_reallocs(), 1u);
   BOOST_TEST_EQ(resp3.value().get_total_msgs(), 1u);

   auto const tmp2 = from_flat(resp2);
   BOOST_TEST_ALL_EQ(resp1.begin(), resp1.end(), tmp2.begin(), tmp2.end());

   auto const tmp3 = from_flat(resp3);
   BOOST_TEST_ALL_EQ(resp1.begin(), resp1.end(), tmp3.begin(), tmp3.end());
}

// The response should be reusable.
void test_reuse()
{
   flat_tree tmp;

   // First use
   error_code ec;
   deserialize(resp3_set, adapt2(tmp), ec);
   BOOST_TEST_EQ(ec, error_code{});

   BOOST_TEST_EQ(tmp.get_reallocs(), 1u);
   BOOST_TEST_EQ(tmp.get_total_msgs(), 1u);

   // Copy to compare after the reuse.
   auto const resp1 = tmp.get_view();
   tmp.clear();

   // Second use
   deserialize(resp3_set, adapt2(tmp), ec);
   BOOST_TEST_EQ(ec, error_code{});

   // No reallocation this time. TODO: check this
   BOOST_TEST_EQ(tmp.get_reallocs(), 1u);
   BOOST_TEST_EQ(tmp.get_total_msgs(), 1u);

   BOOST_TEST_ALL_EQ(resp1.begin(), resp1.end(), tmp.get_view().begin(), tmp.get_view().end());
}

void test_copy_assign()
{
   flat_tree ref1;
   flat_tree ref2;
   flat_tree ref3;
   flat_tree ref4;

   error_code ec;
   deserialize(resp3_set, adapt2(ref1), ec);
   deserialize(resp3_set, adapt2(ref2), ec);
   deserialize(resp3_set, adapt2(ref3), ec);
   deserialize(resp3_set, adapt2(ref4), ec);
   BOOST_TEST_EQ(ec, error_code{});

   // Copy ctor
   flat_tree copy1{ref1};

   // Move ctor
   flat_tree move1{std::move(ref2)};

   // Copy assignment
   flat_tree copy2 = ref1;

   // Move assignment
   flat_tree move2 = std::move(ref3);

   // Assignment
   flat_tree copy3;
   copy3 = ref1;

   // Move assignment
   flat_tree move3;
   move3 = std::move(ref4);

   BOOST_TEST((copy1 == ref1));
   BOOST_TEST((copy2 == ref1));
   BOOST_TEST((copy3 == ref1));

   BOOST_TEST((move1 == ref1));
   BOOST_TEST((move2 == ref1));
   BOOST_TEST((move3 == ref1));
}

}  // namespace

int main()
{
   test_views_are_set();
   test_copy_assign();
   test_reuse();

   return boost::report_errors();
}
