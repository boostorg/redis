//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/config.hpp>
#include <boost/redis/detail/co_connect_fsm.hpp>
#include <boost/redis/detail/coroutine.hpp>
#include <boost/redis/impl/log_utils.hpp>

#include <boost/assert.hpp>
#include <boost/corosio/resolver_results.hpp>

#include <span>
#include <string>

namespace boost::redis::detail {

// Logging
inline void format_tcp_endpoint(const corosio::endpoint& ep, std::string& to)
{
   if (ep.is_v6()) {
      to += '[';
      to += ep.v6_address().to_string();
      to += ']';
   } else {
      to += ep.v4_address().to_string();
   }
   to += ':';
   to += std::to_string(ep.port());
}

template <>
struct log_traits<corosio::endpoint> {
   static inline void log(std::string& to, const corosio::endpoint& value)
   {
      format_tcp_endpoint(value, to);
   }
};

template <>
struct log_traits<std::span<const corosio::resolver_entry>> {
   static inline void log(std::string& to, std::span<const corosio::resolver_entry> value)
   {
      auto iter = value.begin();
      auto end = value.end();

      if (iter != end) {
         format_tcp_endpoint(iter->get_endpoint(), to);
         ++iter;
         for (; iter != end; ++iter) {
            to += ", ";
            format_tcp_endpoint(iter->get_endpoint(), to);
         }
      }
   }
};

co_connect_action co_connect_fsm::resume(
   system::error_code ec,
   std::span<const corosio::resolver_entry> resolver_results)
{
   // Log it
   if (ec) {
      log_info(*lgr_, "Connect: hostname resolution failed: ", ec);
   } else {
      log_debug(*lgr_, "Connect: hostname resolution results: ", resolver_results);
   }

   // Delegate to the regular resume function
   return resume(ec);
}

co_connect_action co_connect_fsm::resume(
   system::error_code ec,
   const corosio::endpoint& selected_endpoint)
{
   // Log it
   if (ec) {
      log_info(*lgr_, "Connect: TCP connect failed: ", ec);
   } else {
      log_debug(*lgr_, "Connect: TCP connect succeeded. Selected endpoint: ", selected_endpoint);
   }

   // Delegate to the regular resume function
   return resume(ec);
}

co_connect_action co_connect_fsm::resume(system::error_code ec)
{
   switch (resume_point_) {
      BOOST_REDIS_CORO_INITIAL

      if (type_ == transport_type::unix_socket) {
         // Connect to the socket
         BOOST_REDIS_YIELD(resume_point_, 1, co_connect_action_type::unix_socket_connect)

         // Log it
         if (ec) {
            log_info(*lgr_, "Connect: UNIX socket connect failed: ", ec);
         } else {
            log_debug(*lgr_, "Connect: UNIX socket connect succeeded");
         }

         // If this failed, we can't continue
         if (ec) {
            return ec;
         }

         // Done
         return system::error_code();
      } else {
         // Resolve names. The continuation needs access to the returned
         // endpoints, and is a specialized resume() that will call this function
         BOOST_REDIS_YIELD(resume_point_, 2, co_connect_action_type::tcp_resolve)

         // If this failed, we can't continue
         if (ec) {
            return ec;
         }

         // Now connect to the endpoints returned by the resolver.
         // This has a specialized resume(), too
         BOOST_REDIS_YIELD(resume_point_, 3, co_connect_action_type::tcp_connect)

         // If this failed, we can't continue
         if (ec) {
            return ec;
         }

         if (type_ == transport_type::tcp_tls) {
            // Perform the TLS handshake
            BOOST_REDIS_YIELD(resume_point_, 6, co_connect_action_type::ssl_handshake)

            // Log it
            if (ec) {
               log_info(*lgr_, "Connect: SSL handshake failed: ", ec);
            } else {
               log_debug(*lgr_, "Connect: SSL handshake succeeded");
            }

            // If this failed, we can't continue
            if (ec) {
               return ec;
            }
         }

         // Done
         return system::error_code();
      }
   }

   BOOST_ASSERT(false);
   return system::error_code();
}

}  // namespace boost::redis::detail
