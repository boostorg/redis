/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <memory>
#include <string>

#include <aedis/detail/write.hpp>
#include <aedis/detail/response_buffers.hpp>

#include "net.hpp"
#include "types.hpp"
#include "request.hpp"

namespace aedis {

/** A class that keeps a connection to the redis server.
*/
class connection : public std::enable_shared_from_this<connection> {
public:
   /** Redis server configuration.
   */
   struct config {
      /// Redis host.
      std::string host;

      /// Redis port.
      std::string port;

      /** The maximum pipeline size. Once this size is exceeded a new
       *  pipeline is added to the queue.
       */
      int max_pipeline_size = 1000;

      /** The maximum pipeline payload size. Once this size is
       *  exceeded a new pipeline is added to the queue.
       */
      int max_payload_size = 10000;
   };

private:
   net::ip::tcp::socket socket_;
   std::string buffer_;
   detail::response_buffers resps_;
   detail::request_queue reqs_;
   bool reconnect_ = false;
   config conf_;
   boost::system::error_code ec_;

   net::awaitable<void> worker_coro(receiver_base& recv);

public:
   /// Contructs a connection.
   connection(
      net::any_io_executor const& ioc,
      config const& conf = config {"127.0.0.1", "6379", 1000, 10000});

   /// Stablishes the connection with the redis server.
   void start(receiver_base& recv);

   /** Adds commands to the ouput queue. The Filler signature must be
    *
    *  void f(request& req)
    */
   template <class Filler>
   bool send(Filler filler)
   {
      auto const empty = std::empty(reqs_);
      if (empty || std::size(reqs_) == 1)
	 reqs_.push({});

      auto const pipeline_size = std::ssize(reqs_.back().req);
      auto const payload_size = std::ssize(reqs_.back().req.payload);
      if (pipeline_size > conf_.max_pipeline_size || payload_size > conf_.max_payload_size)
	 reqs_.push({});

      filler(reqs_.back().req);

      if (empty) {
	 co_spawn(
	    socket_.get_executor(),
	    detail::async_write_all(socket_, reqs_, ec_),
	    net::detached);
      }

      return empty;
   }

   auto queue_size() const noexcept
      { return std::ssize(reqs_); }

   void enable_reconnect() noexcept;
};

} // aedis
