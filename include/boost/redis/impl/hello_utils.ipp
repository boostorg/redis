/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/detail/hello_utils.hpp>

namespace boost::redis::detail {

void setup_hello_request(config const& cfg, request& req)
{
   req.clear();
   if (!cfg.username.empty() && !cfg.password.empty() && !cfg.clientname.empty())
      req.push("HELLO", "3", "AUTH", cfg.username, cfg.password, "SETNAME", cfg.clientname);
   else if (cfg.password.empty() && cfg.clientname.empty())
      req.push("HELLO", "3");
   else if (cfg.clientname.empty())
      req.push("HELLO", "3", "AUTH", cfg.username, cfg.password);
   else
      req.push("HELLO", "3", "SETNAME", cfg.clientname);

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

   auto f = [](auto const& e) {
      switch (e.data_type) {
         case resp3::type::simple_error:
         case resp3::type::blob_error:   return true;
         default:                        return false;
      }
   };

   bool has_error = std::any_of(resp->cbegin(), resp->cend(), f);
   return has_error ? error::resp3_hello : system::error_code();
}

}  // namespace boost::redis::detail
