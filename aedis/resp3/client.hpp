/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <queue>
#include <functional>

#include <aedis/aedis.hpp>

namespace aedis {
namespace resp3 {
namespace experimental {

/**  \brief A high level redis client.
 *   \ingroup classes
 *
 *   This client keeps a connection to the database open.
 */
class client : public std::enable_shared_from_this<client> {
public:
   /// The response adapter type.
   using adapter_type = std::function<void(command, type, std::size_t, std::size_t, char const*, std::size_t, std::error_code&)>;

   /// The type of the message callback.
   using on_message_type = std::function<void(std::error_code ec, command)>;

private:
   using tcp_socket = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::socket>;

   struct request_info {
      // Request size in bytes.
      std::size_t size = 0;

      // The number of commands it contains excluding commands that
      // have push types as responses, see has_push_response.
      std::size_t cmds = 0;
   };

   // Requests payload.
   std::string requests_;

   // The commands contained in the requests.
   std::queue<command> commands_;

   // Info about the requests.
   std::queue<request_info> req_info_;

   // The stream.
   tcp_socket socket_;

   // Timer used to inform the write coroutine that it can write the
   // next message in the output queue.
   net::steady_timer timer_;

   // Response adapter. TODO: Set a default adapter.
   adapter_type adapter_;

   // Message callback. TODO: Set a default callback.
   on_message_type on_msg_;

   // A coroutine that keeps reading the socket. When a message
   // arrives it calls on_message.
   net::awaitable<void> reader();

   // Write coroutine. It is kept suspended until there are messages
   // to be sent.
   net::awaitable<void> writer();

   net::awaitable<void> say_hello();

   // The connection manager. It keeps trying the reconnect to the
   // server when the connection is lost.
   net::awaitable<void> connection_manager();

   /* Prepares the back of the queue to receive further commands. 
    *
    * If true is returned the request in the front of the queue can be
    * sent to the server. See async_write_some.
    */
   bool prepare_next();

public:
   /// Constructor
   client(net::any_io_executor ex);

   /** \brief Starts the client.
    *
    *  Stablishes a connection with the redis server and keeps
    *  waiting for messages to send.
    *  TODO: Rename to prepare.
    */
   void start();

   /** \brief Adds a command to the command queue.
    */
   template <class... Ts>
   void send(command cmd, Ts const&... args);

   /// Sets the response adapter.
   void set_adapter(adapter_type adapter);

   /// Sets the message callback;
   void set_msg_callback(on_message_type on_msg);
};

template <class... Ts>
void client::send(command cmd, Ts const&... args)
{
   auto const can_write = prepare_next();

   auto sr = make_serializer<command>(requests_);
   auto const before = std::size(requests_);
   sr.push(cmd, args...);
   auto const after = std::size(requests_);
   req_info_.front().size += after - before;;

   if (!has_push_response(cmd)) {
      commands_.emplace(cmd);
      ++req_info_.front().cmds;
   }

   if (can_write)
      timer_.cancel_one();
}

} // experimental
} // resp3
} // aedis
