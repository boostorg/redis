/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "user_session.hpp"

namespace aedis
{

user_session::user_session(net::ip::tcp::socket socket)
: socket_(std::move(socket))
, timer_(socket_.get_executor())
   { timer_.expires_at(std::chrono::steady_clock::time_point::max()); }

net::awaitable<void> user_session::writer()
{
   try {
      while (socket_.is_open()) {
	 if (write_msgs_.empty()) {
	    boost::system::error_code ec;
	    co_await timer_.async_wait(redirect_error(net::use_awaitable, ec));
	 } else {
	    co_await net::async_write(socket_, net::buffer(write_msgs_.front()), net::use_awaitable);
	    write_msgs_.pop_front();
	 }
      }
   } catch (std::exception&) {
     stop();
   }
}

void user_session::stop()
{
   socket_.close();
   timer_.cancel();
}

void user_session::deliver(std::string const& msg)
{
   write_msgs_.push_back(msg);
   timer_.cancel_one();
}

void user_session::start(std::function<void(std::string const&)> on_msg)
{
   co_spawn(socket_.get_executor(),
       [self = shared_from_this(), on_msg]{ return self->reader(on_msg); },
       net::detached);

   co_spawn(socket_.get_executor(),
       [self = shared_from_this()]{ return self->writer(); },
       net::detached);
}

net::awaitable<void>
user_session::reader(std::function<void(std::string const&)> on_msg)
{
   try {
      for (std::string msg;;) {
	 auto const n = co_await net::async_read_until(socket_, net::dynamic_buffer(msg, 1024), "\n", net::use_awaitable);
	 on_msg(msg);
	 msg.erase(0, n);
      }
   } catch (std::exception&) {
      stop();
   }
}

} // aedis
