/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_EXPERIMENTAL_SYNC_WRAPPER_HPP
#define AEDIS_EXPERIMENTAL_SYNC_WRAPPER_HPP

#include <mutex>
#include <thread>
#include <memory>

// TODO: Include only asio needed headers.
#include <boost/asio.hpp>
#include <aedis/connection.hpp>
#include <aedis/resp3/request.hpp>

namespace aedis {
namespace experimental {

template <class Connection>
class sync_wrapper {
private:
   boost::asio::io_context ioc{1};
   std::shared_ptr<Connection> db;
   std::thread thread;

   void run(boost::string_view host, boost::string_view port)
   {
      auto handler = [](auto ec)
         { std::clog << ec.message() << std::endl; };

      db->async_run(host, port, handler);
      ioc.run();
   }

public:
   sync_wrapper(boost::string_view host, boost::string_view port)
   : db{std::make_shared<Connection>(ioc)}
   , thread{[this, host, port](){run(host, port);}}
   { }

   ~sync_wrapper()
   {
      ioc.stop();
      thread.join();
   }

   template <class ResponseAdapter>
   std::size_t
   exec(resp3::request const& req, ResponseAdapter adapter, boost::system::error_code& ec)
   {
      std::mutex m;
      std::condition_variable cv;
      bool ready = false;
      std::size_t res = 0;

      auto f = [this, &ec, &res, &m, &cv, &ready, &req, adapter]()
      {
         std::lock_guard lk(m);
         db->async_exec(req, adapter, [&cv, &ready, &res, &ec](auto const& ecp, std::size_t n) {
            ec = ecp;
            res = n;
            ready = true;
            cv.notify_one();
         });
      };

      boost::asio::dispatch(boost::asio::bind_executor(ioc, f));
      std::unique_lock lk(m);
      cv.wait(lk, [&ready]{return ready;});
      return res;
   }

   template <class ResponseAdapter>
   std::size_t exec(resp3::request const& req, ResponseAdapter adapter)
   {
      boost::system::error_code ec;
      auto const res = exec(req, adapter, ec);
      if (ec)
         throw std::system_error(ec);
      return res;
   }
};

#endif

} // experimental
} // aedis
