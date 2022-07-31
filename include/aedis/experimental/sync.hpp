/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_EXPERIMENTAL_SYNC_HPP
#define AEDIS_EXPERIMENTAL_SYNC_HPP

#include <aedis/connection.hpp>
#include <aedis/resp3/request.hpp>

namespace aedis {
namespace experimental {

/** @brief Executes a command.
 *  @ingroup any
 *
 *  This function will block until execution completes.
 *
 *  @param conn The connection.
 *  @param req The request.
 *  @param adapter The response adapter.
 *  @param ec Error code in case of error.
 *  @returns The number of bytes of the response.
 */
template <class Connection, class ResponseAdapter>
std::size_t
exec(
   Connection& conn,
   resp3::request const& req,
   ResponseAdapter adapter,
   boost::system::error_code& ec)
{
   std::mutex mutex;
   std::condition_variable cv;
   bool ready = false;
   std::size_t res = 0;

   auto f = [&conn, &ec, &res, &mutex, &cv, &ready, &req, adapter]()
   {
      conn.async_exec(req, adapter, [&cv, &mutex, &ready, &res, &ec](auto const& ecp, std::size_t n) {
         std::unique_lock ul(mutex);
         ec = ecp;
         res = n;
         ready = true;
         ul.unlock();
         cv.notify_one();
      });
   };

   boost::asio::dispatch(boost::asio::bind_executor(conn.get_executor(), f));
   std::unique_lock lk(mutex);
   cv.wait(lk, [&ready]{return ready;});
   return res;
}

/** @brief Executes a command.
 *  @ingroup any
 *
 *  This function will block until execution completes.
 *
 *  @param conn The connection.
 *  @param req The request.
 *  @param adapter The response adapter.
 *  @throws std::system_error in case of error.
 *  @returns The number of bytes of the response.
 */
template <class Connection, class ResponseAdapter>
std::size_t
exec(Connection& conn, resp3::request const& req, ResponseAdapter adapter)
{
   boost::system::error_code ec;
   auto const res = exec(conn, req, adapter, ec);
   if (ec)
      throw std::system_error(ec);
   return res;
}

} // experimental
} // aedis

#endif // AEDIS_EXPERIMENTAL_SYNC_HPP
