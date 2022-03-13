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

#include <aedis/resp3/type.hpp>
#include <aedis/resp3/response_traits.hpp>

namespace aedis {
namespace generic {

/**  \brief Base class for receivers that use tuple.
 *   \ingroup any
 */
template <class Command, class ...Ts>
class receiver_base {
private:
   using tuple_type = std::tuple<Ts...>;
   using variant_type = boost::mp11::mp_rename<boost::mp11::mp_transform<resp3::response_traits_t, tuple_type>, std::variant>;

   tuple_type resps_;
   std::array<variant_type, std::tuple_size<tuple_type>::value> adapters_;
   bool on_transaction_ = false;

   virtual void on_read_impl(Command) {}
   virtual void on_push_impl() {}
   virtual void on_write_impl(std::size_t) {}
   virtual int to_tuple_idx_impl(Command) { return 0;}

public:
   receiver_base()
      { resp3::adapter::detail::assigner<std::tuple_size<tuple_type>::value - 1>::assign(adapters_, resps_); }

   template <class T>
   auto& get() { return std::get<T>(resps_);};

   template <class T>
   auto const& get() const { return std::get<T>(resps_);};

   template <class T>
   constexpr int index_of() const {return boost::mp11::mp_find<tuple_type, T>::value;}

   void
   on_resp3(
      Command cmd,
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

   void on_read(Command cmd)
   {
      if (cmd == Command::discard)
         on_transaction_ = false;

      if (on_transaction_)
         return;

      on_read_impl(cmd);
   }

   void on_write(std::size_t n)
   {
      on_write_impl(n);
   }

   void on_push()
   {
      on_push_impl();
   }

   int to_tuple_index(Command cmd)
   {
      if (cmd == Command::multi) {
         on_transaction_ = true;
         return -1;
      }

      if (cmd == Command::exec)
         on_transaction_ = false;

      if (on_transaction_)
         return -1;

      return to_tuple_idx_impl(cmd);
   }
};

} // generic
} // aedis
