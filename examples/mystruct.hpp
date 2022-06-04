/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <string>
#include <iterator>
#include <cstdint>
#include <iostream>
#include <algorithm>

#include <aedis/aedis.hpp>

// Arbitrary struct to de/serialize.
struct mystruct {
   std::int32_t x;
   std::string y;
};

// TODO: Use json instead.
// Serializes mystruct
void to_bulk(std::string& to, mystruct const& obj)
{
   using aedis::resp3::type;
   using aedis::resp3::add_header;
   using aedis::resp3::add_separator;

   auto const size = sizeof obj.x + obj.y.size();
   add_header(to, type::blob_string, size);
   auto const* p = reinterpret_cast<char const*>(&obj.x);
   std::copy(p, p + sizeof obj.x, std::back_inserter(to));
   std::copy(std::cbegin(obj.y), std::cend(obj.y), std::back_inserter(to));
   add_separator(to);
}

// Deserialize the struct.
void from_string(mystruct& obj, boost::string_view sv, boost::system::error_code& ec)
{
   char* p = reinterpret_cast<char*>(&obj.x);
   std::copy(std::cbegin(sv), std::cbegin(sv) + sizeof obj.x, p);
   std::copy(std::cbegin(sv) + sizeof obj.x, std::cend(sv), std::back_inserter(obj.y));
}

std::ostream& operator<<(std::ostream& os, mystruct const& obj)
{
   os << "x: " << obj.x << ", y: " << obj.y;
   return os;
}

bool operator<(mystruct const& a, mystruct const& b)
{
   return std::tie(a.x, a.y) < std::tie(b.x, b.y);
}
