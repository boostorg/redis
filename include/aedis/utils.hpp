/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <iostream>

namespace aedis { namespace resp {

template <class Iter>
void print(Iter begin, Iter end, char const* p)
{
  std::cout << p << ": ";
   for (; begin != end; ++begin)
     std::cout << *begin << " ";
   std::cout << std::endl;
}

template <class Range>
void print(Range const& v, char const* p = "")
{
   using std::cbegin;
   using std::cend;
   print(cbegin(v), cend(v), p);
}

} // resp
} // aedis
