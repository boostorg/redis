/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <string>
#include <iterator>
#include <cstdint>
#include <iostream>
#include <algorithm>

#include <boost/asio/connect.hpp>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

namespace net = boost::asio;
namespace resp3 = aedis::resp3;

using aedis::resp3::type;
using aedis::redis::command;
using aedis::generic::make_serializer;
using aedis::adapter::adapt;
using net::dynamic_buffer;
using net::ip::tcp;

// Arbitrary struct to de/serialize.
struct mystruct {
   std::int32_t x;
   std::string y;
};

// Serializes mystruct
void to_bulk(std::string& to, mystruct const& obj)
{
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

int main()
{
   try {
      net::io_context ioc;
      tcp::resolver resv{ioc};
      auto const res = resv.resolve("127.0.0.1", "6379");
      tcp::socket socket{ioc};
      net::connect(socket, res);

      // This struct will be serialized and stored on Redis.
      mystruct in{42, "Some string"};

      // Creates and sends a request to redis.
      std::string request;
      auto sr = make_serializer(request);
      sr.push(command::hello, 3);
      sr.push(command::set, "key", in);
      sr.push(command::get, "key");
      sr.push(command::quit);
      net::write(socket, net::buffer(request));

      // Responses
      mystruct out;

      // Reads the responses to all commands in the request.
      std::string buffer;
      resp3::read(socket, dynamic_buffer(buffer)); // hello
      resp3::read(socket, dynamic_buffer(buffer)); // set
      resp3::read(socket, dynamic_buffer(buffer), adapt(out)); // get
      resp3::read(socket, dynamic_buffer(buffer)); // quit

      // Should be equal to what has been sent above.
      std::cout << out << std::endl;

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
      exit(EXIT_FAILURE);
   }
}
