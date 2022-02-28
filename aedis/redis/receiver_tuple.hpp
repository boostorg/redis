/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <array>
#include <variant>
#include <tuple>

#include <boost/mp11.hpp>

#include <aedis/redis/command.hpp>
#include <aedis/resp3/type.hpp>
#include <aedis/resp3/response_traits.hpp>

namespace aedis {
namespace redis {

/**  \brief Base class for receivers that use tuple.
 *   \ingroup any
 */
template <class ...Ts>
class receiver_tuple {
private:
   using tuple_type = std::tuple<Ts...>;
   using variant_type = boost::mp11::mp_rename<boost::mp11::mp_transform<resp3::response_traits_t, tuple_type>, std::variant>;

   tuple_type resps_;
   std::array<variant_type, std::tuple_size<tuple_type>::value> adapters_;

protected:
   virtual int to_tuple_index(command cmd) { return 0; }

   template <class T>
   auto& get() { return std::get<T>(resps_);};

   template <class T>
   auto const& get() const { return std::get<T>(resps_);};

  template <class T>
  constexpr int index_of() const {return boost::mp11::mp_find<tuple_type, T>::value;}

public:
   receiver_tuple()
      { resp3::adapter::detail::assigner<std::tuple_size<tuple_type>::value - 1>::assign(adapters_, resps_); }

   void
   on_resp3(
      command cmd,
      resp3::type t,
      std::size_t aggregate_size,
      std::size_t depth,
      char const* data,
      std::size_t size,
      std::error_code& ec)
   {
      auto const i = to_tuple_index(cmd);
      if (i == -1)
        return;

      std::visit([&](auto& arg){arg(t, aggregate_size, depth, data, size, ec);}, adapters_[i]);
   }

   virtual void on_read(command) {}
   virtual void on_write(std::size_t) {}
};

} // redis
} // aedis
