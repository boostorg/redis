/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>
#include <vector>
#include <tuple>
#include <array>

#include <boost/mp11.hpp>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

#include "lib/net_utils.hpp"

namespace resp3 = aedis::resp3;
using aedis::redis::command;
using aedis::redis::make_serializer;
using resp3::adapt;
using resp3::node;
using resp3::is_aggregate;
using resp3::response_traits;
using resp3::type;

namespace net = aedis::net;
using net::async_write;
using net::buffer;
using net::dynamic_buffer;

// Reads the response to a transaction in a general format that is
// suitable for all kinds of responses, but which users will most
// likely have to convert into their own desired format.
net::awaitable<void> nested_response1()
{
   try {
      auto socket = co_await connect();

      auto list  = {"one", "two", "three"};

      std::string request;
      auto sr = make_serializer(request);
      sr.push(command::hello, 3);
      sr.push(command::flushall);
      sr.push(command::multi);
      sr.push(command::ping, "Some message");
      sr.push(command::incr, "incr-key");
      sr.push_range(command::rpush, "list-key", std::cbegin(list), std::cend(list));
      sr.push(command::lrange, "list-key", 0, -1);
      sr.push(command::exec);
      sr.push(command::quit);
      co_await async_write(socket, buffer(request));

      // Expected responses.
      std::vector<node> exec;

      // Reads the response.
      std::string buffer;
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // hello
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // flushall
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // multi
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // ping
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // incr
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // rpush
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // lrange
      co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt(exec));
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // quit

      // Prints the response.
      std::cout << "General format:\n";
      for (auto const& e: exec) std::cout << e << "\n";

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
      exit(EXIT_FAILURE);
   }
}

template <std::size_t N>
struct helper {
  template <class T1, class T2>
  static void assign(T1& dest, T2& from)
  {
     dest[N].template emplace<N>(adapt(std::get<N>(from)));
     helper<N - 1>::assign(dest, from);
  }
};

template <>
struct helper<0> {
  template <class T1, class T2>
  static void assign(T1& dest, T2& from)
  {
     dest[0] = adapt(std::get<0>(from));
  }
};

// Same as above but parses the responses directly in their final data
// structures.
//
// WARNING: This example is not interesting for most users, it is an
// adavanced feature meant for people with strong performance need.

template <class T>
using response_traits_t = typename response_traits<T>::adapter_type;

// Adapts the responses above to a read operation.
template <class Tuple>
class flat_transaction_adapter {
private:
   using variant_type =
      boost::mp11::mp_rename<boost::mp11::mp_transform<response_traits_t, Tuple>, std::variant>;

   std::size_t i_ = 0;
   std::size_t aggregate_size_ = 0;
   std::array<variant_type, std::tuple_size<Tuple>::value> adapters_;

public:
   flat_transaction_adapter(Tuple& r)
      { helper<std::tuple_size<Tuple>::value - 1>::assign(adapters_, r); }

   void count(type t, std::size_t aggregate_size, std::size_t depth)
   {
      if (depth == 1) {
         if (is_aggregate(t))
            aggregate_size_ = aggregate_size;
         else
            ++i_;

         return;
      }

      if (--aggregate_size_ == 0)
         ++i_;
   }

   void
   operator()(
      type t,
      std::size_t aggregate_size,
      std::size_t depth,
      char const* data,
      std::size_t size,
      std::error_code& ec)
   {
      if (depth == 0) {
         // aggregate_size must be equal to the tuple size. Check
         // this and add a new error code.
         return; // We are interested in the size of the transaction.
      }

      std::visit([&](auto& arg){arg(t, aggregate_size, depth, data, size, ec);}, adapters_[i_]);
      count(t, aggregate_size, depth);
   }
};

template <class Tuple>
auto make_adapter(Tuple& t)
{
   return flat_transaction_adapter<Tuple>(t);
}

net::awaitable<void> nested_response2()
{
   try {
      auto socket = co_await connect();

      auto list  = {"one", "two", "three"};

      std::string request;
      auto sr = make_serializer(request);
      sr.push(command::hello, 3);
      sr.push(command::flushall);

      // Adds a transaction
      sr.push(command::multi);
      sr.push(command::ping, "Some message");
      sr.push(command::incr, "incr1-key");
      sr.push_range(command::rpush, "list-key", std::cbegin(list), std::cend(list));
      sr.push(command::lrange, "list-key", 0, -1);
      sr.push(command::incr, "incr2-key");
      sr.push(command::exec);

      sr.push(command::quit);
      co_await async_write(socket, buffer(request));

      // Expected responses.
      std::tuple<std::string, int, int, std::vector<std::string>, int> execs;

      // Reads the response.
      std::string buffer;
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // hello
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // flushall
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // multi
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // ping
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // incr
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // rpush
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // lrange
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // incr
      co_await resp3::async_read(socket, dynamic_buffer(buffer), make_adapter(execs));
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // quit

      // Prints the response to the transaction.
      std::cout << "ping: " << std::get<0>(execs) << "\n";
      std::cout << "incr1: " << std::get<1>(execs) << "\n";
      std::cout << "rpush: " << std::get<2>(execs) << "\n";
      std::cout << "lrange: ";
      for (auto const& e: std::get<3>(execs)) std::cout << e << " ";
      std::cout << "\n";
      std::cout << "incr2: " << std::get<4>(execs) << "\n";

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
      exit(EXIT_FAILURE);
   }
}
int main()
{
   net::io_context ioc;
   co_spawn(ioc, nested_response1(), net::detached);
   co_spawn(ioc, nested_response2(), net::detached);
   ioc.run();
}
