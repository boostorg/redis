/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/config.hpp>
#include <boost/redis/detail/hello_utils.hpp>

namespace boost::redis::detail {

void setup_hello_request(config const& cfg, request& req)
{
   // Which parts of the command should we send?
   // Don't send AUTH if the user is the default and the password is empty.
   // Other users may have empty passwords.
   // Note that this is just an optimization.
   bool send_auth = !(cfg.username.empty() || (cfg.username == "default" && cfg.password.empty()));
   bool send_setname = !cfg.clientname.empty();

   req.clear();

   if (cfg.use_hello) {
      // Gather everything we can in a HELLO command
      if (send_auth && send_setname)
         req.push("HELLO", "3", "AUTH", cfg.username, cfg.password, "SETNAME", cfg.clientname);
      else if (send_auth)
         req.push("HELLO", "3", "AUTH", cfg.username, cfg.password);
      else if (send_setname)
         req.push("HELLO", "3", "SETNAME", cfg.clientname);
      else
         req.push("HELLO", "3");
   } else {
      // The user doesn't want us to use the HELLO command.
      // Send any required auth/client name commands separately.
      if (send_auth)
         req.push("AUTH", cfg.username, cfg.password);
      if (send_setname)
         req.push("CLIENT", "SETNAME", cfg.clientname);
   }

   // SELECT is independent of HELLO
   if (cfg.database_index && cfg.database_index.value() != 0)
      req.push("SELECT", cfg.database_index.value());
}

void clear_response(generic_response& res)
{
   if (res.has_value())
      res->clear();
   else
      res.emplace();
}

system::error_code check_hello_response(system::error_code io_ec, const generic_response& resp)
{
   if (io_ec)
      return io_ec;

   if (resp.has_error())
      return error::resp3_hello;

   return system::error_code();
}

}  // namespace boost::redis::detail
