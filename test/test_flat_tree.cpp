/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/adapter/adapt.hpp>
#include <boost/redis/resp3/flat_tree.hpp>
#include <boost/redis/resp3/node.hpp>
#include <boost/redis/resp3/type.hpp>

#include <boost/assert/source_location.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/span.hpp>

#include "print_node.hpp"

#include <algorithm>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using boost::redis::adapter::adapt2;
using boost::redis::adapter::result;
using boost::redis::resp3::tree;
using boost::redis::resp3::flat_tree;
using boost::redis::generic_flat_response;
using boost::redis::resp3::type;
using boost::redis::resp3::detail::deserialize;
using boost::redis::resp3::node;
using boost::redis::resp3::node_view;
using boost::redis::resp3::to_string;
using boost::redis::response;
using boost::system::error_code;

namespace {

constexpr std::string_view
   resp3_set = "~6\r\n+orange\r\n+apple\r\n+one\r\n+two\r\n+three\r\n+orange\r\n";

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

void add_nodes(
   flat_tree& to,
   std::string_view data,
   boost::source_location loc = BOOST_CURRENT_LOCATION)
{
   error_code ec;
   deserialize(data, adapt2(to), ec);
   if (!BOOST_TEST_EQ(ec, error_code{}))
      std::cerr << "Called from " << loc << std::endl;
}

void check_nodes(
   const flat_tree& tree,
   boost::span<const node_view> expected,
   boost::source_location loc = BOOST_CURRENT_LOCATION)
{
   if (!BOOST_TEST_ALL_EQ(
          tree.get_view().begin(),
          tree.get_view().end(),
          expected.begin(),
          expected.end()))
      std::cerr << "Called from " << loc << std::endl;
}

void test_default_constructor()
{
   flat_tree t;

   check_nodes(t, {});
   BOOST_TEST_EQ(t.data_size(), 0u);
   BOOST_TEST_EQ(t.get_reallocs(), 0u);
   BOOST_TEST_EQ(t.get_total_msgs(), 0u);
}

// Adding nodes works, even when reallocations happen.
// Empty nodes don't cause trouble
void test_add_nodes()
{
   flat_tree t;

   // Add a bunch of nodes. Single allocation. Some nodes are empty.
   add_nodes(t, "*2\r\n+hello\r\n+world\r\n");
   std::vector<node_view> expected_nodes{
      {type::array,         2u, 0u, ""     },
      {type::simple_string, 1u, 1u, "hello"},
      {type::simple_string, 1u, 1u, "world"},
   };
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 10u);
   BOOST_TEST_EQ(t.data_capacity(), 512u);
   BOOST_TEST_EQ(t.get_reallocs(), 1u);
   BOOST_TEST_EQ(t.get_total_msgs(), 1u);

   // Capacity will have raised to 512 bytes, at least. Add some more without reallocations
   add_nodes(t, "$3\r\nbye\r\n");
   expected_nodes.push_back({type::blob_string, 1u, 0u, "bye"});
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 13u);
   BOOST_TEST_EQ(t.data_capacity(), 512u);
   BOOST_TEST_EQ(t.get_reallocs(), 1u);
   BOOST_TEST_EQ(t.get_total_msgs(), 2u);

   // Add nodes above the first reallocation threshold. Node strings are still valid
   const std::string long_value(600u, 'a');
   add_nodes(t, "+" + long_value + "\r\n");
   expected_nodes.push_back({type::simple_string, 1u, 0u, long_value});
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 613u);
   BOOST_TEST_EQ(t.data_capacity(), 1024u);
   BOOST_TEST_EQ(t.get_reallocs(), 2u);
   BOOST_TEST_EQ(t.get_total_msgs(), 3u);

   // Add some more nodes, still within the reallocation threshold
   add_nodes(t, "+some_other_value\r\n");
   expected_nodes.push_back({type::simple_string, 1u, 0u, "some_other_value"});
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 629u);
   BOOST_TEST_EQ(t.data_capacity(), 1024u);
   BOOST_TEST_EQ(t.get_reallocs(), 2u);
   BOOST_TEST_EQ(t.get_total_msgs(), 4u);

   // Add some more, causing another reallocation
   add_nodes(t, "+" + long_value + "\r\n");
   expected_nodes.push_back({type::simple_string, 1u, 0u, long_value});
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 1229u);
   BOOST_TEST_EQ(t.data_capacity(), 2048u);
   BOOST_TEST_EQ(t.get_reallocs(), 3u);
   BOOST_TEST_EQ(t.get_total_msgs(), 5u);
}

// Strings are really copied into the object
void test_add_nodes_copies()
{
   flat_tree t;

   // Place the message in dynamic memory
   constexpr std::string_view const_msg = "+some_long_value_for_a_node\r\n";
   std::unique_ptr<char[]> data{new char[100]{}};
   std::copy(const_msg.begin(), const_msg.end(), data.get());

   // Add nodes pointing into this message
   add_nodes(t, data.get());

   // Invalidate the original message
   data.reset();

   // Check
   std::vector<node_view> expected_nodes{
      {type::simple_string, 1u, 0u, "some_long_value_for_a_node"},
   };
   check_nodes(t, expected_nodes);
}

// Reallocations happen only when we would exceed capacity
void test_add_nodes_capacity_limit()
{
   flat_tree t;

   // Add a node to reach capacity 512
   add_nodes(t, "+hello\r\n");
   BOOST_TEST_EQ(t.data_size(), 5u);
   BOOST_TEST_EQ(t.data_capacity(), 512u);

   // Fill the rest of the capacity
   add_nodes(t, "+" + std::string(507u, 'b') + "\r\n");
   BOOST_TEST_EQ(t.data_size(), 512u);
   BOOST_TEST_EQ(t.data_capacity(), 512u);

   // Adding an empty node here doesn't change capacity
   add_nodes(t, "_\r\n");
   BOOST_TEST_EQ(t.data_size(), 512u);
   BOOST_TEST_EQ(t.data_capacity(), 512u);

   // Adding more data causes a reallocation
   add_nodes(t, "+a\r\n");
   BOOST_TEST_EQ(t.data_size(), 513u);
   BOOST_TEST_EQ(t.data_capacity(), 1024);

   // Same goes for the next capacity limit
   add_nodes(t, "+" + std::string(511u, 'c') + "\r\n");
   BOOST_TEST_EQ(t.data_size(), 1024);
   BOOST_TEST_EQ(t.data_capacity(), 1024);

   // Reallocation
   add_nodes(t, "+u\r\n");
   BOOST_TEST_EQ(t.data_size(), 1025u);
   BOOST_TEST_EQ(t.data_capacity(), 2048u);

   // This would continue
   add_nodes(t, "+" + std::string(1024u, 'd') + "\r\n");
   BOOST_TEST_EQ(t.data_size(), 2049u);
   BOOST_TEST_EQ(t.data_capacity(), 4096u);
}

// It's no problem if a node is big enough to surpass several reallocation limits
void test_add_nodes_big_node()
{
   flat_tree t;

   // Add a bunch of nodes. Single allocation. Some nodes are empty.
   const std::string long_value(1500u, 'h');
   add_nodes(t, "+" + long_value + "\r\n");
   std::vector<node_view> expected_nodes{
      {type::simple_string, 1u, 0u, long_value},
   };
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 1500u);
   BOOST_TEST_EQ(t.data_capacity(), 2048u);
   BOOST_TEST_EQ(t.get_reallocs(), 1u);
   BOOST_TEST_EQ(t.get_total_msgs(), 1u);
}

// --- Move
// void test_move_ctor()
// {
//    flat_tree t;

//    add_nodes(t, "$2\r\n+hello\r\n+world\r\n");
//    flat_tree t2{std::move(t)};

//    check_nodes(t2, {});
//    BOOST_TEST_EQ(t2.data_size(), 0u);
//    BOOST_TEST_EQ(t2.get_reallocs(), 0u);
//    BOOST_TEST_EQ(t2.get_total_msgs(), 0u);
// }

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
   test_add_nodes();
   test_add_nodes_copies();
   test_add_nodes_capacity_limit();
   test_add_nodes_big_node();

   test_default_constructor();

   test_views_are_set();
   test_copy_assign();
   test_reuse();

   return boost::report_errors();
}
