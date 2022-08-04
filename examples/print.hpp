/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
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

void print_aggr(std::vector<aedis::resp3::node<std::string>>& v)
{
   auto const m = aedis::resp3::element_multiplicity(v.front().data_type);
   for (auto i = 0lu; i < m * v.front().aggregate_size; ++i)
      std::cout << v[i + 1].value << " ";
   std::cout << "\n";
   v.clear();
}

template <class T>
void print(std::vector<T> const& cont)
{
   for (auto const& e: cont) std::cout << e << " ";
   std::cout << "\n";
}

template <class T>
void print(std::set<T> const& cont)
{
   for (auto const& e: cont) std::cout << e << "\n";
}

template <class T, class U>
void print(std::map<T, U> const& cont)
{
   for (auto const& e: cont)
      std::cout << e.first << ": " << e.second << "\n";
}

void print(std::string const& e)
{
   std::cout << e << std::endl;
}

void print_push(std::vector<aedis::resp3::node<std::string>>& resp)
{
   std::cout
      << "Push type: "   << resp.at(1).value << "\n"
      << "Channel: " << resp.at(2).value << "\n"
      << "Message: " << resp.at(3).value << "\n"
      << std::endl;
}
