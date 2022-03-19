/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>
#include <stdlib.h>

template <class T>
void check_equal(T const& a, T const& b, std::string const& msg = "")
{
   if (a == b) {
     std::cout << "Success: " << msg << std::endl;
   } else {
     std::cout << "Error: " << msg << std::endl;
     exit(EXIT_FAILURE);
   }
}

void check_error(boost::system::error_code ec)
{
   if (ec) {
      std::cout << ec << std::endl;
      exit(EXIT_FAILURE);
   }
}

template <class T>
void check_empty(T const& t)
{
   if (!std::empty(t)) {
      std::cout << "Error: Not empty" << std::endl;
      exit(EXIT_FAILURE);
   }
}

