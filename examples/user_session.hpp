/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_USER_SESSION_HPP
#define AEDIS_USER_SESSION_HPP

#include <functional>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/redirect_error.hpp>

// An example user session.

namespace aedis
{

// Base class for user sessions.
struct user_session_base {
  virtual ~user_session_base() {}
  virtual void deliver(std::string const& msg) = 0;
};

class user_session:
   public user_session_base,
   public std::enable_shared_from_this<user_session> {
public:
   user_session(boost::asio::ip::tcp::socket socket)
   : socket_(std::move(socket))
   , timer_(socket_.get_executor())
      { timer_.expires_at(std::chrono::steady_clock::time_point::max()); }

   void start(std::function<void(std::string const&)> on_msg)
   {
      co_spawn(socket_.get_executor(),
          [self = shared_from_this(), on_msg]{ return self->reader(on_msg); },
          boost::asio::detached);

      co_spawn(socket_.get_executor(),
          [self = shared_from_this()]{ return self->writer(); },
          boost::asio::detached);
   }

   void deliver(std::string const& msg)
   {
      write_msgs_.push_back(msg);
      timer_.cancel_one();
   }

private:
   boost::asio::awaitable<void>
   reader(std::function<void(std::string const&)> on_msg)
   {
      try {
         for (std::string msg;;) {
            auto const n = co_await boost::asio::async_read_until(socket_, boost::asio::dynamic_buffer(msg, 1024), "\n", boost::asio::use_awaitable);
            on_msg(msg);
            msg.erase(0, n);
         }
      } catch (std::exception&) {
         stop();
      }
   }

   boost::asio::awaitable<void> writer()
   {
      try {
         while (socket_.is_open()) {
            if (write_msgs_.empty()) {
               boost::system::error_code ec;
               co_await timer_.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
            } else {
               co_await boost::asio::async_write(socket_, boost::asio::buffer(write_msgs_.front()), boost::asio::use_awaitable);
               write_msgs_.pop_front();
            }
         }
      } catch (std::exception&) {
        stop();
      }
   }

   void stop()
   {
      socket_.close();
      timer_.cancel();
   }

   boost::asio::ip::tcp::socket socket_;
   boost::asio::steady_timer timer_;
   std::deque<std::string> write_msgs_;
};

} // aedis
#endif // AEDIS_USER_SESSION_HPP
