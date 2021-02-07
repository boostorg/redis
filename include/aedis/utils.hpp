/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <iostream>

namespace aedis {

template <class Iter>
void print(Iter begin, Iter end, char const* p)
{
   if (p)
     std::cout << p << ": ";
   for (; begin != end; ++begin)
     std::cout << *begin << " ";
   std::cout << std::endl;
}

template <class Range>
void print(Range const& v, char const* p = nullptr)
{
   using std::cbegin;
   using std::cend;
   print(cbegin(v), cend(v), p);
}

inline
void print_command_raw(std::string const& data, int n)
{
  for (int i = 0; i < n; ++i) {
    if (data[i] == '\n') {
      std::cout << "\\n";
      continue;
    }
    if (data[i] == '\r') {
      std::cout << "\\r";
      continue;
    }
    std::cout << data[i];
  }
}

} // aedis
