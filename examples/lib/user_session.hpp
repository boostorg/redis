/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <functional>

#include <aedis/net.hpp>

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
   user_session(net::ip::tcp::socket socket);
   void start(std::function<void(std::string const&)> on_msg);
   void deliver(std::string const& msg);

private:
   net::awaitable<void> reader(std::function<void(std::string const&)> on_msg);
   net::awaitable<void> writer();
   void stop();
   net::ip::tcp::socket socket_;
   net::steady_timer timer_;
   std::deque<std::string> write_msgs_;
};

} // aedis
