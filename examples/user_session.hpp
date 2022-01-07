#include <aedis/aedis.hpp>

#include <vector>
#include <iostream>

// Defines an example user session.

namespace net = aedis::net;

using aedis::command;
using aedis::resp3::client_base;

struct user_session_base;

// Holds the information that is needed when a response to a
// request arrives. See client_base.hpp for more details on the
// required fields in this struct.
struct response_id {
   // The redis command that corresponds to this command. 
   command cmd = command::unknown;

   // Pointer to the response.
   std::shared_ptr<std::string> resp;

   // The pointer to the session the request belongs to.
   std::weak_ptr<user_session_base> session =
      std::shared_ptr<user_session_base>{nullptr};
};

// Base class for user sessions.
struct user_session_base {
  virtual ~user_session_base() {}
  virtual void deliver(std::string const& msg) = 0;
};

using client_base_type = client_base<response_id>;

class user_session:
   public user_session_base,
   public std::enable_shared_from_this<user_session> {
public:
   using tcp_socket = aedis::net::use_awaitable_t<>::as_default_on_t<aedis::net::ip::tcp::socket>;

   user_session(tcp_socket socket, std::shared_ptr<client_base_type> rclient)
   : socket_(std::move(socket))
   , timer_(socket_.get_executor())
   , rclient_{rclient}
   {
      timer_.expires_at(std::chrono::steady_clock::time_point::max());
   }

   template <class Filler>
   void start(Filler filler)
   {
      co_spawn(socket_.get_executor(),
          [self = shared_from_this(), filler]{ return self->reader(filler); },
          net::detached);

      co_spawn(socket_.get_executor(),
          [self = shared_from_this()]{ return self->writer(); },
          net::detached);
   }

   void deliver(std::string const& msg) override
   {
      write_msgs_.push_back(msg);
      timer_.cancel_one();
   }

private:
   template <class Filler>
   net::awaitable<void> reader(Filler filler)
   {
     try {
       for (std::string msg;;) {
         auto const n =
            co_await net::async_read_until(socket_, net::dynamic_buffer(msg, 1024), "\n");

         auto filler2 = [filler, s = std::move(msg)](auto& req)
	    { filler(req, s); };

         rclient_->send(filler2);

         msg.erase(0, n);
       }
     } catch (std::exception&) {
       stop();
     }
   }

   net::awaitable<void> writer()
   {
     try {
       while (socket_.is_open()) {
         if (write_msgs_.empty()) {
           boost::system::error_code ec;
           co_await timer_.async_wait(redirect_error(net::use_awaitable, ec));
         } else {
           co_await net::async_write(socket_, net::buffer(write_msgs_.front()));
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

   tcp_socket socket_;
   net::steady_timer timer_;
   std::deque<std::string> write_msgs_;
   std::shared_ptr<client_base_type> rclient_;
};

