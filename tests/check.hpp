/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <iostream>
#include <stdlib.h>

template <class T>
void expect_eq(T const& a, T const& b, std::string const& msg = "")
{
   if (a == b) {
     std::cout << "Success: " << msg << std::endl;
   } else {
     std::cout << "Error: " << msg << std::endl;
     exit(EXIT_FAILURE);
   }
}

template <class T>
void expect_error(boost::system::error_code a, T expected = {})
{
   if (a == expected) {
      if (a)
         std::cout << "Success: " << a.message() << " (" << a.category().name() << ")" << std::endl;
   } else {
      std::cout << "Error: " << a.message() << " (" << a.category().name() << ")" << std::endl;
      exit(EXIT_FAILURE);
   }
}

template <class T>
void check_empty(T const& t)
{
   if (t.empty()) {
      //std::cout << "Success: " << std::endl;
   } else {
      std::cout << "Error: Not empty" << std::endl;
      exit(EXIT_FAILURE);
   }
}

