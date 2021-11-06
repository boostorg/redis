/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>

#include <aedis/aedis.hpp>

#include "types.hpp"
#include "utils.ipp"

using namespace aedis;

/* Pushes three commands in a request, write and read them in the same response
 * object.
 */
net::awaitable<void> ping1()
{
   auto socket = co_await make_connection();
   resp3::stream<tcp_socket> stream{std::move(socket)};

   resp3::request req;
   req.push(command::hello, 3);
   req.push(command::ping);
   req.push(command::quit);
   co_await stream.async_write(req);

   resp3::response resp;
   co_await stream.async_read(resp);
   co_await stream.async_read(resp);
   co_await stream.async_read(resp);

   std::cout << resp << std::endl;
}

/* Like obove but uses a while loop to read the commands.
 */
net::awaitable<void> ping2()
{
   auto socket = co_await make_connection();
   resp3::stream<tcp_socket> stream{std::move(socket)};

   resp3::request req;
   req.push(command::hello, 3);
   req.push(command::ping);
   req.push(command::quit);
   co_await stream.async_write(req);

   while (!std::empty(req.commands)) {
      resp3::response resp;
      co_await stream.async_read(resp);
      std::cout << req.commands.front() << ":\n" << resp << std::endl;
      req.commands.pop();
   }
}

/* A more elaborate way of doing what has been done above where we send a new
 * command only after the last one has arrived. This is usually the starting
 * point for more complex applications. Here we also separate the application
 * logic out out the coroutine for clarity.
 */
bool prepare_next(std::queue<resp3::request>& reqs)
{
   if (std::empty(reqs)) {
      reqs.push({});
      return true;
   }

   if (std::size(reqs) == 1) {
      reqs.push({});
      return false;
   }

   return false;
}

void process_response3(std::queue<resp3::request>& requests, resp3::response& resp)
{
   std::cout << requests.front().commands.front() << ":\n" << resp << std::endl;

   switch (requests.front().commands.front()) {
      case command::hello:
         prepare_next(requests);
         requests.back().push(command::ping);
         break;
      case command::ping:
         prepare_next(requests);
         requests.back().push(command::quit);
         break;
      default: {};
   }
}

net::awaitable<void> ping3()
{
   auto socket = co_await make_connection();
   resp3::stream<tcp_socket> stream{std::move(socket)};

   std::queue<resp3::request> requests;
   requests.push({});
   requests.back().push(command::hello, 3);

   while (!std::empty(requests)) {
      co_await stream.async_write(requests.front());
      while (!std::empty(requests.front().commands)) {
         resp3::response resp;
         co_await stream.async_read(resp);
         process_response3(requests, resp);
         requests.front().commands.pop();
      }

      requests.pop();
   }
}

/* More realistic usage example. Like above but we keep reading from
 * the socket in order to implement a full-duplex communication
 */

class state : public std::enable_shared_from_this<state> {
private:
   resp3::stream<tcp_socket> stream_;
   std::queue<resp3::request> requests_;

public:
   explicit state(tcp_socket socket)
   : stream_(std::move(socket))
   { }

   void start()
   {
     co_spawn(stream_.get_executor(),
         [self = shared_from_this()]{ return self->reader(); },
         net::detached);

     for (auto i = 0; i < 100; ++i) {
	std::string msg = "Writer ";
	msg += std::to_string(i);
	co_spawn(stream_.get_executor(),
	    [msg, self = shared_from_this()]{ return self->writer(msg); },
	    net::detached);
     }
   }

   void process_push(resp3::response const& resp)
   {
      std::cout << resp << std::endl;
   }

   void process_resp(resp3::response const& resp)
   {
      std::cout
	 << requests_.front().commands.front()
	 << ":\n" << resp << std::endl;
   }

   // This reader supports many features of the resp3 protocol.
   net::awaitable<void> reader()
   {
      requests_.push({});
      requests_.back().push(command::hello, 3);
      requests_.back().push(command::subscribe, "channel");

      // Writes and reads continuosly from the socket.
      for (;;) {
	 // Writes the first outstanding connection.
	 co_await stream_.async_write(requests_.front());

	 // Keeps reading while there is no message to be sent.
	 do {
	    // We have to consume the responses to all commands in the
	    // request.
	    do {
	       // Reads the response to one command.
	       resp3::response resp;
	       co_await stream_.async_read(resp);
	       if (resp.get_type() == resp3::type::push) {
		  // Server push.
		  process_push(resp);
	       } else {
		  // Prints the command and the response to it.
		  process_resp(resp);
		  requests_.front().commands.pop();
	       }
	    } while (!std::empty(requests_) && !std::empty(requests_.front().commands));

	    // We may exit the loop above either because we are done
	    // with the response or because we received a server push
	    // while the queue was empty.
	    if (!std::empty(requests_))
	       requests_.pop();

	 } while (std::empty(requests_));
      }
   }

   net::awaitable<void> writer(std::string message)
   {
      auto ex = co_await aedis::net::this_coro::executor;
      net::steady_timer t{ex};

      while (stream_.next_layer().is_open()) {
	 t.expires_after(std::chrono::milliseconds{100});
	 co_await t.async_wait(net::use_awaitable);

	 auto const can_write = prepare_next(requests_);
	 requests_.back().push(command::publish, "channel", message);
	 requests_.back().push(command::publish, "channel", message);
	 requests_.back().push(command::publish, "channel", message);
	 if (can_write)
	    co_await stream_.async_write(requests_.front());
      }
   }

};

net::awaitable<void> ping4()
{
   auto socket = co_await make_connection();
   std::make_shared<state>(std::move(socket))->start();
}

int main()
{
   net::io_context ioc;
   co_spawn(ioc, ping1(), net::detached);
   co_spawn(ioc, ping2(), net::detached);
   co_spawn(ioc, ping3(), net::detached);
   co_spawn(ioc, ping4(), net::detached);
   ioc.run();
}
