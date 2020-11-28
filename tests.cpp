/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "aedis.hpp"

namespace net = aedis::net;
using tcp = net::ip::tcp;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<tcp::socket>;

namespace this_coro = net::this_coro;

using namespace net;
using namespace aedis;

void check_equal(
   std::vector<std::string> const& a,
   std::vector<std::string> const& b)
{
   auto const r =
      std::equal( std::cbegin(a)
                , std::cend(a)
                , std::cbegin(b));

   if (r)
     std::cout << "Success" << std::endl;
   else
     std::cout << "Error" << std::endl;
}

awaitable<void> test1()
{
   auto ex = co_await this_coro::executor;

   tcp::resolver resv(ex);
   auto const rr = resv.resolve("127.0.0.1", "6379");

   tcp_socket socket {ex};
   co_await async_connect(socket, rr);

   std::vector<std::vector<std::string>> r;
   resp::pipeline p;

   p.flushall();
   r.push_back({"OK"});

   p.ping();
   r.push_back({"PONG"});

   p.rpush("a", std::list<std::string>{"1" ,"2", "3"});
   r.push_back({"3"});

   p.rpush("a", std::vector<std::string>{"4" ,"5", "6"});
   r.push_back({"6"});

   p.rpush("a", std::set<std::string>{"7" ,"8", "9"});
   r.push_back({"9"});

   p.rpush("a", std::initializer_list<std::string>{"10" ,"11", "12"});
   r.push_back({"12"});

   p.lrange("a");
   r.push_back({"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12"});

   p.lrange("a", 4, -5);
   r.push_back({"5", "6", "7", "8"});

   p.ltrim("a", 4, -5);
   r.push_back({"OK"});

   p.lpop("a");
   r.push_back({"5"});

   p.lpop("a");
   r.push_back({"6"});

   p.quit();
   r.push_back({"OK"});

   co_await async_write(socket, buffer(p.payload));

   resp::buffer buffer;
   for (auto const& o : r) {
     resp::response res;
     co_await resp::async_read(socket, buffer, res);
     check_equal(res.res, o);
   }
}

awaitable<void> resp3()
{
   auto ex = co_await this_coro::executor;

   tcp::resolver resv(ex);
   auto const rr = resv.resolve("127.0.0.1", "6379");

   tcp_socket socket {ex};
   co_await async_connect(socket, rr);

   std::vector<std::vector<std::string>> r;
   resp::pipeline p;

   p.hello("3");
   r.push_back({"OK"});

   p.quit();
   r.push_back({"OK"});

   co_await async_write(socket, buffer(p.payload));

   resp::buffer buffer;
   for (auto const& o : r) {
     resp::response res;
     co_await resp::async_read(socket, buffer, res);
     resp::print(res.res);
     check_equal(res.res, o);
   }
}

struct initiate_async_receive {
  using executor_type = net::system_executor;

  std::string const& payload;

  executor_type get_executor() noexcept
    { return net::system_executor(); }

  template <
    class ReadHandler,
    class MutableBufferSequence>
  void operator()(ReadHandler&& handler,
      MutableBufferSequence const& buffers) const
  {
    boost::system::error_code ec;
    if (std::size(buffers) == 0) {
      handler(ec, 0);
      return;
    }

    assert(std::size(buffers) != 0);
    auto begin = boost::asio::buffer_sequence_begin(buffers);
    assert(std::size(payload) <= std::size(*begin));
    char* p = static_cast<char*>(begin->data());
    std::copy(std::begin(payload), std::end(payload), p);
    handler(ec, std::size(payload));
  }
};

struct test_stream {
   std::string const& payload;

   using executor_type = net::system_executor;

   template<
      class MutableBufferSequence,
      class ReadHandler =
          net::default_completion_token_t<executor_type>
    >
    void async_read_some(
        MutableBufferSequence const& buffers,
        ReadHandler&& handler)
    {
      return net::async_initiate<ReadHandler,
        void (boost::system::error_code, std::size_t)>(
          initiate_async_receive{payload}, handler, buffers);
    }

    executor_type get_executor() noexcept
      { return net::system_executor(); }
};

struct test_handler {
   void operator()(boost::system::error_code ec) const
   {
      if (ec)
         std::cout << ec.message() << std::endl;
   }
};

void send(std::string cmd)
{
   net::io_context ioc;
   session<tcp::socket> s {ioc};

   s.send(std::move(cmd));
   s.disable_reconnect();

   s.run();
   ioc.run();
}

void offline()
{
   // Redis answer - Expected vector.
   std::vector<std::pair<std::string, std::vector<std::string>>> payloads
   { {{"+OK\r\n"},                                         {"OK"}}
   , {{":3\r\n"},                                          {"3"}}
   , {{"*3\r\n$3\r\none\r\n$3\r\ntwo\r\n$5\r\nthree\r\n"}, {"one", "two", "three"}}
   , {{"$2\r\nhh\r\n"},                                    {"hh"}}
   , {{"-Error\r\n"},                                      {"Error"}}
   };

   for (auto const& e : payloads) {
     test_stream ts {e.first};
     resp::buffer buffer;
     resp::response res;
     async_read(ts, buffer, res, test_handler {});
     if (e.second != res.res)
       std::cout << "Error" << std::endl;
     else
       std::cout << "Success: Offline tests." << std::endl;
   }
}

int main(int argc, char* argv[])
{
   //send(ping());
   offline();
   io_context ioc {1};
   co_spawn(ioc, test1(), detached);
   //co_spawn(ioc, resp3(), detached);
   ioc.run();
}

