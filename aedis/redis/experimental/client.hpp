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
#include <aedis/redis/command.hpp>

namespace aedis {
namespace resp3 {
namespace experimental {

/**  \brief A high level redis client.
 *   \ingroup any
 *
 *   This Redis client keeps a connection to the database open and
 *   uses it for all communication with Redis. For examples on how to
 *   use see the examples chat_room.cpp, echo_server.cpp and redis_client.cpp.
 *
 *   \remarks This class reuses its internal buffers for requests and
 *   for reading Redis responses. With time it will allocate less and
 *   less.
 */
class client : public std::enable_shared_from_this<client> {
public:
   /** \brief The extended response adapter type.
    *
    *  The difference between the adapter and extended_adapter
    *  concepts is that the extended get a command redis::parameter.
    */
   using extented_adapter_type = std::function<void(redis::command, type, std::size_t, std::size_t, char const*, std::size_t, std::error_code&)>;

   /// The type of the message callback.
   using on_message_type = std::function<void(std::error_code ec, redis::command)>;

   /// The type of the socket used by the client.
   //using socket_type = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::socket>;
   using socket_type = net::ip::tcp::socket;

private:
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
   std::queue<redis::command> commands_;

   // Info about the requests.
   std::queue<request_info> req_info_;

   // The stream.
   socket_type socket_;

   // Timer used to inform the write coroutine that it can write the
   // next message in the output queue.
   net::steady_timer timer_;

   // Response adapter.
   extented_adapter_type extended_adapter_ = [](redis::command, type, std::size_t, std::size_t, char const*, std::size_t, std::error_code&) {};

   // Message callback.
   on_message_type on_msg_ = [](std::error_code ec, redis::command) {};

   // Set when the writer coroutine should stop.
   bool stop_writer_ = false;

   // A coroutine that keeps reading the socket. When a message
   // arrives it calls on_message.
   net::awaitable<void> reader();

   // Write coroutine. It is kept suspended until there are messages
   // to be sent.
   net::awaitable<void> writer();

   /* Prepares the back of the queue to receive further commands. 
    *
    * If true is returned the request in the front of the queue can be
    * sent to the server. See async_write_some.
    */
   bool prepare_next();

public:
   /** \brief Client constructor.
    *
    *  Constructos the client from an executor.
    *
    *  \param ex The executor.
    */
   client(net::any_io_executor ex);

   /// Returns the executor used for I/O with Redis.
   auto get_executor() {return socket_.get_executor();}

   /** \brief Starts communication with Redis.
    *
    *  This functions will send the hello command to Redis and spawn
    *  the read and write coroutines.
    *
    *  \param socket A socket that is connected to redis.
    *
    *  \returns This function returns an awaitable on which users should \c
    *  co_await. When the communication with Redis is lost the
    *  coroutine will finally co_return.
    */
   net::awaitable<void> engage(socket_type socket);

   /** \brief Adds a command to the command queue.
    *
    *  \sa serializer.hpp
    */
   template <class... Ts>
   void send(redis::command cmd, Ts const&... args);

   /// Sets an extended response adapter.
   void set_extended_adapter(extented_adapter_type adapter);

   /// Sets the message callback;
   void set_msg_callback(on_message_type on_msg);
};

template <class... Ts>
void client::send(redis::command cmd, Ts const&... args)
{
   auto const can_write = prepare_next();

   auto sr = redis::make_serializer(requests_);
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
