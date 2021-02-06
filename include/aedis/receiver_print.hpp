/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <memory>
#include <iostream>

#include "type.hpp"
#include "utils.hpp"
#include "response.hpp"
#include "request.hpp"

namespace aedis { namespace resp {

template <class Event>
class receiver_base {
public:
   using event_type = Event;

   virtual void on_array(command cmd, Event ev, response_array::data_type& v) noexcept
      { print(v); }
   virtual void on_push(command cmd, Event ev, response_array::data_type& v) noexcept
      { print(v); }
   virtual void on_map(command cmd, Event ev, response_array::data_type& v) noexcept
      { print(v); }
   virtual void on_set(command cmd, Event ev, response_array::data_type& v) noexcept
      { print(v); }
   virtual void on_attribute(command cmd, Event ev, response_array::data_type& v) noexcept
      { print(v); }
   virtual void on_simple_string(command cmd, Event ev, response_simple_string::data_type& v) noexcept
      { std::cout << v << std::endl; }
   virtual void on_simple_error(command cmd, Event ev, response_simple_error::data_type& v) noexcept
      { std::cout << v << std::endl; }
   virtual void on_number(command cmd, Event ev, response_number::data_type& v) noexcept
      { std::cout << v << std::endl; }
   virtual void on_double(command cmd, Event ev, response_double::data_type& v) noexcept
      { std::cout << v << std::endl; }
   virtual void on_big_number(command cmd, Event ev, response_big_number::data_type& v) noexcept
      { std::cout << v << std::endl; }
   virtual void on_boolean(command cmd, Event ev, response_bool::data_type& v) noexcept
      { std::cout << v << std::endl; }
   virtual void on_blob_string(command cmd, Event ev, response_blob_string::data_type& v) noexcept
      { std::cout << v << std::endl; }
   virtual void on_blob_error(command cmd, Event ev, response_blob_error::data_type& v) noexcept
      { std::cout << v << std::endl; }
   virtual void on_verbatim_string(command cmd, Event ev, response_verbatim_string::data_type& v) noexcept
      { std::cout << v << std::endl; }
   virtual void on_streamed_string_part(command cmd, Event ev, response_streamed_string_part::data_type& v) noexcept
      { std::cout << v << std::endl; }
};

template <class Event>
class connection :
   public std::enable_shared_from_this<connection<Event>> {
private:
   net::steady_timer st_;
   tcp::resolver resv_;
   tcp::socket socket_;
   std::queue<request<Event>> reqs_;

   template <class Receiver>
   net::awaitable<void>
   reconnect_loop(Receiver& recv)
   {
      try {
	 auto ex = co_await net::this_coro::executor;
	 auto const r = resv_.resolve("127.0.0.1", "6379");
	 co_await async_connect(socket_, r, net::use_awaitable);
	 resp::async_writer(socket_, reqs_, st_, net::detached);
	 co_await co_spawn(
	    ex,
	    resp::async_reader(socket_, recv, reqs_),
	    net::use_awaitable);
      } catch (std::exception const& e) {
	 std::cout << e.what() << std::endl;
	 socket_.close();
	 st_.cancel();
      }
   }

public:
   using event_type = Event;

   connection(net::io_context& ioc)
   : st_{ioc}
   , resv_{ioc}
   , socket_{ioc}
   , reqs_ (resp::make_request_queue<Event>())
   { }

   template <class Receiver>
   void start(Receiver& recv)
   {
      net::co_spawn(
	 socket_.get_executor(),
         [self = this->shared_from_this(), recv] () mutable { return self->reconnect_loop(recv); },
         net::detached);
   }

   template <class Filler>
   void send(Filler filler)
      { queue_writer(reqs_, filler, st_); }

   auto& requests() {return reqs_;}
   auto const& requests() const {return reqs_;}
};

}
}
