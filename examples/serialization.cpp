/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <string>
#include <iostream>
#include <charconv>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

namespace net = aedis::net;
using aedis::redis::command;
using aedis::redis::receiver_tuple;
using aedis::redis::index_of;
using client_type = aedis::redis::client<net::detached_t::as_default_on_t<aedis::net::ip::tcp::socket>>;

struct mystruct {
  int a;
  int b;
};

std::string to_string(mystruct const& obj)
{
   return std::to_string(obj.a) + '\t' + std::to_string(obj.b);
}

// Deserializes avoiding temporary copies.
void
from_string(
   mystruct& obj,
   char const* data,
   std::size_t size,
   std::error_code& ec)
{
   auto const* end = data + size;
   auto const* pos = std::find(data, end, '\t');
   assert(pos != end); // Or use your own error code.

   auto const res1 = std::from_chars(data, pos, obj.a);
   if (res1.ec != std::errc()) {
      ec = std::make_error_code(res1.ec);
      return;
   }

   auto const res2 = std::from_chars(pos + 1, end, obj.b);
   if (res2.ec != std::errc()) {
      ec = std::make_error_code(res2.ec);
      return;
   }
}

struct receiver : receiver_tuple<mystruct> {
private:
   client_type* db_;

   int to_tuple_index(command cmd) override
   {
      switch (cmd) {
         case command::get: return index_of<mystruct>();
         default: return -1;
      }
   }

public:
   receiver(client_type& db) : db_{&db} {}

   void on_read(command cmd) override
   {
      switch (cmd) {
         case command::hello:
         {
            db_->send(command::set, "serialization-key", mystruct{21, 22});
            db_->send(command::get, "serialization-key");
            db_->send(command::quit);
         } break;

         case command::get:
         {
            std::cout << "a: " << get<mystruct>().a << "\n"
                      << "b: " << get<mystruct>().b << "\n";
         } break;

         default:;
      }
   }
};

int main()
{
   net::io_context ioc;
   client_type db(ioc.get_executor());
   receiver recv{db};
   db.async_run(recv);
   ioc.run();
}
