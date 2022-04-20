/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <map>
#include <set>
#include <vector>
#include <string>
#include <iostream>

#include <aedis/resp3/type.hpp>
#include <aedis/resp3/node.hpp>

// Some functions to make the examples less repetitive.

namespace net = boost::asio;
using aedis::resp3::node;
using aedis::adapter::adapt;
using aedis::adapter::adapter_t;
using aedis::redis::command;
using aedis::generic::client;

void print_and_clear_aggregate(std::vector<aedis::resp3::node<std::string>>& v)
{
   auto const m = aedis::resp3::element_multiplicity(v.front().data_type);
   for (auto i = 0lu; i < m * v.front().aggregate_size; ++i)
      std::cout << v[i + 1].value << " ";
   std::cout << "\n";
   v.clear();
}

void print_and_clear(std::set<std::string>& cont)
{
   for (auto const& e: cont) std::cout << e << " ";
   std::cout << "\n";
   cont.clear();
}

void print_and_clear(std::map<std::string, std::string>& cont)
{
   for (auto const& e: cont)
      std::cout << e.first << ": " << e.second << "\n";
   cont.clear();
}

