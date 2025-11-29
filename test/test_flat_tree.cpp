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

// --- Adding nodes ---
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

// --- Reserving space ---
// The usual case, calling it before using it
void test_reserve()
{
   flat_tree t;

   t.reserve(1024u, 5u);
   check_nodes(t, {});
   BOOST_TEST_EQ(t.get_view().capacity(), 5u);
   BOOST_TEST_EQ(t.data_size(), 0u);
   BOOST_TEST_EQ(t.data_capacity(), 1024);
   BOOST_TEST_EQ(t.get_reallocs(), 1u);
   BOOST_TEST_EQ(t.get_total_msgs(), 0u);

   // Adding some nodes now works
   add_nodes(t, "+hello\r\n");
   std::vector<node_view> expected_nodes{
      {type::simple_string, 1u, 0u, "hello"},
   };
   check_nodes(t, expected_nodes);
}

// Reserving space uses the same allocation thresholds
void test_reserve_not_power_of_2()
{
   flat_tree t;

   // First threshold at 512
   t.reserve(200u, 5u);
   BOOST_TEST_EQ(t.data_capacity(), 512u);
   BOOST_TEST_EQ(t.get_reallocs(), 1u);

   // Second threshold at 1024
   t.reserve(600u, 5u);
   BOOST_TEST_EQ(t.data_capacity(), 1024u);
   BOOST_TEST_EQ(t.get_reallocs(), 2u);
}

// Requesting a capacity below the current one does nothing
void test_reserve_below_current_capacity()
{
   flat_tree t;

   // Reserving with a zero capacity does nothing
   t.reserve(0u, 0u);
   BOOST_TEST_EQ(t.data_capacity(), 0u);
   BOOST_TEST_EQ(t.get_reallocs(), 0u);

   // Increase capacity
   t.reserve(400u, 5u);
   BOOST_TEST_EQ(t.data_capacity(), 512u);
   BOOST_TEST_EQ(t.get_reallocs(), 1u);

   // Reserving again does nothing
   t.reserve(400u, 5u);
   t.reserve(512u, 5u);
   t.reserve(0u, 5u);
   BOOST_TEST_EQ(t.data_capacity(), 512u);
   BOOST_TEST_EQ(t.get_reallocs(), 1u);
}

// Reserving might reallocate. If there are nodes, strings remain valid
void test_reserve_with_data()
{
   flat_tree t;

   // Add a bunch of nodes, and then reserve
   add_nodes(t, "*2\r\n+hello\r\n+world\r\n");
   t.reserve(1000u, 10u);

   // Check
   std::vector<node_view> expected_nodes{
      {type::array,         2u, 0u, ""     },
      {type::simple_string, 1u, 1u, "hello"},
      {type::simple_string, 1u, 1u, "world"},
   };
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 10u);
   BOOST_TEST_EQ(t.data_capacity(), 1024u);
   BOOST_TEST_EQ(t.get_reallocs(), 2u);
   BOOST_TEST_EQ(t.get_total_msgs(), 1u);
}

// --- Clear ---
void test_clear()
{
   flat_tree t;

   // Add a bunch of nodes, then clear
   add_nodes(t, "*2\r\n+hello\r\n+world\r\n");
   t.clear();

   // Nodes are no longer there, but memory hasn't been fred
   check_nodes(t, {});
   BOOST_TEST_EQ(t.data_size(), 0u);
   BOOST_TEST_EQ(t.data_capacity(), 512u);
   BOOST_TEST_EQ(t.get_reallocs(), 1u);
   BOOST_TEST_EQ(t.get_total_msgs(), 0u);
}

// Clearing an empty tree doesn't cause trouble
void test_clear_empty()
{
   flat_tree t;
   t.clear();

   check_nodes(t, {});
   BOOST_TEST_EQ(t.data_size(), 0u);
   BOOST_TEST_EQ(t.data_capacity(), 0u);
   BOOST_TEST_EQ(t.get_reallocs(), 0u);
   BOOST_TEST_EQ(t.get_total_msgs(), 0u);
}

// With clear, memory can be reused
// The response should be reusable.
void test_clear_reuse()
{
   flat_tree t;

   // First use
   add_nodes(t, "~6\r\n+orange\r\n+apple\r\n+one\r\n+two\r\n+three\r\n+orange\r\n");
   std::vector<node_view> expected_nodes{
      {type::set,           6u, 0u, ""      },
      {type::simple_string, 1u, 1u, "orange"},
      {type::simple_string, 1u, 1u, "apple" },
      {type::simple_string, 1u, 1u, "one"   },
      {type::simple_string, 1u, 1u, "two"   },
      {type::simple_string, 1u, 1u, "three" },
      {type::simple_string, 1u, 1u, "orange"},
   };
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.get_reallocs(), 1u);
   BOOST_TEST_EQ(t.get_total_msgs(), 1u);

   // Second use
   t.clear();
   add_nodes(t, "*2\r\n+hello\r\n+world\r\n");
   expected_nodes = {
      {type::array,         2u, 0u, ""     },
      {type::simple_string, 1u, 1u, "hello"},
      {type::simple_string, 1u, 1u, "world"},
   };
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.get_reallocs(), 1u);
   BOOST_TEST_EQ(t.get_total_msgs(), 1u);
}

// --- Default ctor ---
void test_default_constructor()
{
   flat_tree t;

   check_nodes(t, {});
   BOOST_TEST_EQ(t.data_size(), 0u);
   BOOST_TEST_EQ(t.get_reallocs(), 0u);
   BOOST_TEST_EQ(t.get_total_msgs(), 0u);
}

// --- Copy ctor ---
void test_copy_ctor()
{
   // Setup
   auto t = std::make_unique<flat_tree>();
   add_nodes(*t, "*2\r\n+hello\r\n+world\r\n");
   std::vector<node_view> expected_nodes{
      {type::array,         2u, 0u, ""     },
      {type::simple_string, 1u, 1u, "hello"},
      {type::simple_string, 1u, 1u, "world"},
   };

   // Construct, then destroy the original copy
   flat_tree t2{*t};
   t.reset();

   // Check
   check_nodes(t2, expected_nodes);
   BOOST_TEST_EQ(t2.data_size(), 10u);
   BOOST_TEST_EQ(t2.data_capacity(), 512u);
   BOOST_TEST_EQ(t2.get_reallocs(), 1u);
   BOOST_TEST_EQ(t2.get_total_msgs(), 1u);
}

// Copying an empty tree doesn't cause problems
void test_copy_ctor_empty()
{
   flat_tree t;
   flat_tree t2{t};
   check_nodes(t2, {});
   BOOST_TEST_EQ(t2.data_size(), 0u);
   BOOST_TEST_EQ(t2.data_capacity(), 0u);
   BOOST_TEST_EQ(t2.get_reallocs(), 0u);
   BOOST_TEST_EQ(t2.get_total_msgs(), 0u);
}

// Copying an object that has no elements but some capacity doesn't cause trouble
void test_copy_ctor_empty_with_capacity()
{
   flat_tree t;
   t.reserve(300u, 8u);

   flat_tree t2{t};
   check_nodes(t2, {});
   BOOST_TEST_EQ(t2.data_size(), 0u);
   BOOST_TEST_EQ(t2.data_capacity(), 0u);
   BOOST_TEST_EQ(t2.get_reallocs(), 0u);
   BOOST_TEST_EQ(t2.get_total_msgs(), 0u);
}

// Copying an object with more capacity than required adjusts its capacity
void test_copy_ctor_adjust_capacity()
{
   // Setup
   flat_tree t;
   add_nodes(t, "+hello\r\n");
   std::vector<node_view> expected_nodes{
      {type::simple_string, 1u, 0u, "hello"},
   };

   // Cause reallocations
   t.reserve(1000u, 10u);
   t.reserve(2000u, 10u);
   t.reserve(4000u, 10u);

   // Copy
   flat_tree t2{t};

   // The target object has the minimum required capacity,
   // and the number of reallocs has been reset
   check_nodes(t2, expected_nodes);
   BOOST_TEST_EQ(t2.data_size(), 5u);
   BOOST_TEST_EQ(t2.data_capacity(), 512u);
   BOOST_TEST_EQ(t2.get_reallocs(), 1u);
   BOOST_TEST_EQ(t2.get_total_msgs(), 1u);
}

// --- Move ctor ---
void test_move_ctor()
{
   flat_tree t;
   add_nodes(t, "*2\r\n+hello\r\n+world\r\n");

   flat_tree t2{std::move(t)};

   std::vector<node_view> expected_nodes{
      {type::array,         2u, 0u, ""     },
      {type::simple_string, 1u, 1u, "hello"},
      {type::simple_string, 1u, 1u, "world"},
   };
   check_nodes(t2, expected_nodes);
   BOOST_TEST_EQ(t2.data_size(), 10u);
   BOOST_TEST_EQ(t2.data_capacity(), 512u);
   BOOST_TEST_EQ(t2.get_reallocs(), 1u);
   BOOST_TEST_EQ(t2.get_total_msgs(), 1u);
}

// Moving an empty object doesn't cause trouble
void test_move_ctor_empty()
{
   flat_tree t;

   flat_tree t2{std::move(t)};

   check_nodes(t2, {});
   BOOST_TEST_EQ(t2.data_size(), 0u);
   BOOST_TEST_EQ(t2.data_capacity(), 0u);
   BOOST_TEST_EQ(t2.get_reallocs(), 0u);
   BOOST_TEST_EQ(t2.get_total_msgs(), 0u);
}

// Moving an object with capacity but no data doesn't cause trouble
void test_move_ctor_with_capacity()
{
   flat_tree t;
   t.reserve(1000u, 10u);

   flat_tree t2{std::move(t)};

   check_nodes(t2, {});
   BOOST_TEST_EQ(t2.data_size(), 0u);
   BOOST_TEST_EQ(t2.data_capacity(), 1024u);
   BOOST_TEST_EQ(t2.get_reallocs(), 1u);
   BOOST_TEST_EQ(t2.get_total_msgs(), 0u);
}

// --- Copy assignment ---
void test_copy_assign()
{
   flat_tree t;
   add_nodes(t, "+some_data\r\n");

   auto t2 = std::make_unique<flat_tree>();
   add_nodes(*t2, "*2\r\n+hello\r\n+world\r\n");

   t = *t2;

   // Delete the source object, to check that we copied the contents
   t2.reset();

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
}

// The lhs is empty and doesn't have any capacity
void test_copy_assign_target_empty()
{
   flat_tree t;

   flat_tree t2;
   add_nodes(t2, "+hello\r\n");

   t = t2;

   std::vector<node_view> expected_nodes{
      {type::simple_string, 1u, 0u, "hello"},
   };
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 5u);
   BOOST_TEST_EQ(t.data_capacity(), 512u);
   BOOST_TEST_EQ(t.get_reallocs(), 1u);
   BOOST_TEST_EQ(t.get_total_msgs(), 1u);
}

// If the target doesn't have enough capacity, a reallocation happens
void test_copy_assign_target_not_enough_capacity()
{
   flat_tree t;
   add_nodes(t, "+hello\r\n");

   const std::string big_node(2000u, 'a');
   flat_tree t2;
   add_nodes(t2, "+" + big_node + "\r\n");

   t = t2;

   std::vector<node_view> expected_nodes{
      {type::simple_string, 1u, 0u, big_node},
   };
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 2000u);
   BOOST_TEST_EQ(t.data_capacity(), 2048u);
   BOOST_TEST_EQ(t.get_reallocs(), 2u);  // initial + assignment
   BOOST_TEST_EQ(t.get_total_msgs(), 1u);
}

// If the source of the assignment is empty, nothing bad happens
void test_copy_assign_source_empty()
{
   flat_tree t;
   add_nodes(t, "+hello\r\n");

   flat_tree t2;

   t = t2;

   check_nodes(t, {});
   BOOST_TEST_EQ(t.data_size(), 0u);
   BOOST_TEST_EQ(t.data_capacity(), 512u);  // capacity is kept
   BOOST_TEST_EQ(t.get_reallocs(), 1u);
   BOOST_TEST_EQ(t.get_total_msgs(), 0u);
}

// If the source of the assignment has capacity but no data, we're OK
void test_copy_assign_source_with_capacity()
{
   flat_tree t;
   add_nodes(t, "+hello\r\n");

   flat_tree t2;
   t2.reserve(1000u, 4u);
   t2.reserve(4000u, 8u);

   t = t2;

   check_nodes(t, {});
   BOOST_TEST_EQ(t.data_size(), 0u);
   BOOST_TEST_EQ(t.data_capacity(), 512u);  // capacity is kept
   BOOST_TEST_EQ(t.get_reallocs(), 1u);     // not propagated
   BOOST_TEST_EQ(t.get_total_msgs(), 0u);
}

// If the source of the assignment has data with extra capacity
// and a reallocation is needed, the minimum amount of space is allocated
void test_copy_assign_source_with_extra_capacity()
{
   flat_tree t;

   flat_tree t2;
   add_nodes(t2, "+hello\r\n");
   t2.reserve(4000u, 8u);

   t = t2;

   std::vector<node_view> expected_nodes{
      {type::simple_string, 1u, 0u, "hello"},
   };
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 5u);
   BOOST_TEST_EQ(t.data_capacity(), 512u);
   BOOST_TEST_EQ(t.get_reallocs(), 1u);
   BOOST_TEST_EQ(t.get_total_msgs(), 1u);
}

void test_copy_assign_both_empty()
{
   flat_tree t;
   flat_tree t2;

   t = t2;

   check_nodes(t, {});
   BOOST_TEST_EQ(t.data_size(), 0u);
   BOOST_TEST_EQ(t.data_capacity(), 0u);
   BOOST_TEST_EQ(t.get_reallocs(), 0u);
   BOOST_TEST_EQ(t.get_total_msgs(), 0u);
}

// Self-assignment doesn't cause trouble
void test_copy_assign_self()
{
   flat_tree t;
   add_nodes(t, "+hello\r\n");

   const auto& tref = t;
   t = tref;

   std::vector<node_view> expected_nodes{
      {type::simple_string, 1u, 0u, "hello"},
   };
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 5u);
   BOOST_TEST_EQ(t.data_capacity(), 512u);
   BOOST_TEST_EQ(t.get_reallocs(), 1u);
   BOOST_TEST_EQ(t.get_total_msgs(), 1u);
}

// --- Move assignment ---
void test_move_assign()
{
   flat_tree t;
   add_nodes(t, "+some_data\r\n");

   flat_tree t2;
   add_nodes(t2, "*2\r\n+hello\r\n+world\r\n");

   t = std::move(t2);

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
}

// The lhs is empty and doesn't have any capacity
void test_move_assign_target_empty()
{
   flat_tree t;

   flat_tree t2;
   add_nodes(t2, "+hello\r\n");

   t = std::move(t2);

   std::vector<node_view> expected_nodes{
      {type::simple_string, 1u, 0u, "hello"},
   };
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 5u);
   BOOST_TEST_EQ(t.data_capacity(), 512u);
   BOOST_TEST_EQ(t.get_reallocs(), 1u);
   BOOST_TEST_EQ(t.get_total_msgs(), 1u);
}

// If the source of the assignment is empty, nothing bad happens
void test_move_assign_source_empty()
{
   flat_tree t;
   add_nodes(t, "+hello\r\n");

   flat_tree t2;

   t = std::move(t2);

   check_nodes(t, {});
   BOOST_TEST_EQ(t.data_size(), 0u);
   BOOST_TEST_EQ(t.data_capacity(), 0u);
   BOOST_TEST_EQ(t.get_reallocs(), 0u);
   BOOST_TEST_EQ(t.get_total_msgs(), 0u);
}

// If both source and target are empty, nothing bad happens
void test_move_assign_both_empty()
{
   flat_tree t;

   flat_tree t2;

   t = std::move(t2);

   check_nodes(t, {});
   BOOST_TEST_EQ(t.data_size(), 0u);
   BOOST_TEST_EQ(t.data_capacity(), 0u);
   BOOST_TEST_EQ(t.get_reallocs(), 0u);
   BOOST_TEST_EQ(t.get_total_msgs(), 0u);
}

// --- Comparison ---
void test_comparison_different()
{
   flat_tree t;
   add_nodes(t, "+some_data\r\n");

   flat_tree t2;
   add_nodes(t2, "*2\r\n+hello\r\n+world\r\n");

   BOOST_TEST_NOT(t == t2);
   BOOST_TEST(t != t2);
   BOOST_TEST_NOT(t2 == t);
   BOOST_TEST(t2 != t);
}

// The only difference is node types
void test_comparison_different_node_types()
{
   flat_tree t;
   add_nodes(t, "+hello\r\n");

   flat_tree t2;
   add_nodes(t2, "$5\r\nhello\r\n");

   BOOST_TEST_NOT(t == t2);
   BOOST_TEST(t != t2);
}

void test_comparison_equal()
{
   flat_tree t;
   add_nodes(t, "+some_data\r\n");

   flat_tree t2;
   add_nodes(t2, "+some_data\r\n");

   BOOST_TEST(t == t2);
   BOOST_TEST_NOT(t != t2);
}

// Allocations are not taken into account when comparing
void test_comparison_equal_reallocations()
{
   const std::string big_node(2000u, 'a');
   flat_tree t;
   t.reserve(100u, 5u);
   add_nodes(t, "+" + big_node + "\r\n");
   BOOST_TEST_EQ(t.get_reallocs(), 2u);

   flat_tree t2;
   t2.reserve(2048u, 5u);
   add_nodes(t2, "+" + big_node + "\r\n");
   BOOST_TEST_EQ(t2.get_reallocs(), 1u);

   BOOST_TEST(t == t2);
   BOOST_TEST_NOT(t != t2);
}

// Capacity is not taken into account when comparing
void test_comparison_equal_capacity()
{
   flat_tree t;
   add_nodes(t, "+hello\r\n");

   flat_tree t2;
   t2.reserve(2048u, 5u);
   add_nodes(t2, "+hello\r\n");

   BOOST_TEST(t == t2);
   BOOST_TEST_NOT(t != t2);
}

// Empty containers don't cause trouble
void test_comparison_empty()
{
   flat_tree t;
   add_nodes(t, "$5\r\nhello\r\n");

   flat_tree tempty, tempty2;

   BOOST_TEST_NOT(t == tempty);
   BOOST_TEST(t != tempty);

   BOOST_TEST_NOT(tempty == t);
   BOOST_TEST(tempty != t);

   BOOST_TEST(tempty == tempty2);
   BOOST_TEST_NOT(tempty != tempty2);
}

// Self comparisons don't cause trouble
void test_comparison_self()
{
   flat_tree t;
   add_nodes(t, "$5\r\nhello\r\n");

   flat_tree tempty;

   BOOST_TEST(t == t);
   BOOST_TEST_NOT(t != t);

   BOOST_TEST(tempty == tempty);
   BOOST_TEST_NOT(tempty != tempty);
}

}  // namespace

int main()
{
   test_add_nodes();
   test_add_nodes_copies();
   test_add_nodes_capacity_limit();
   test_add_nodes_big_node();

   test_reserve();
   test_reserve_not_power_of_2();
   test_reserve_below_current_capacity();
   test_reserve_with_data();

   test_clear();
   test_clear_empty();
   test_clear_reuse();

   test_default_constructor();

   test_copy_ctor();
   test_copy_ctor_empty();
   test_copy_ctor_empty_with_capacity();
   test_copy_ctor_adjust_capacity();

   test_move_ctor();
   test_move_ctor_empty();
   test_move_ctor_with_capacity();

   test_move_assign();
   test_move_assign_target_empty();
   test_move_assign_source_empty();
   test_move_assign_both_empty();

   test_copy_assign();
   test_copy_assign_target_empty();
   test_copy_assign_target_not_enough_capacity();
   test_copy_assign_source_empty();
   test_copy_assign_source_with_capacity();
   test_copy_assign_source_with_extra_capacity();
   test_copy_assign_both_empty();
   test_copy_assign_self();

   test_comparison_different();
   test_comparison_different_node_types();
   test_comparison_equal();
   test_comparison_equal_reallocations();
   test_comparison_equal_capacity();
   test_comparison_empty();
   test_comparison_self();

   return boost::report_errors();
}
