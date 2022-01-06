#include <aedis/aedis.hpp>

#include <vector>
#include <iostream>

// Defines an example user session.

namespace net = aedis::net;

using aedis::command;
using aedis::resp3::node;
using aedis::resp3::response_traits;
using aedis::resp3::client_base;

// Base class for user sessions.
struct user_session_base {
  virtual ~user_session_base() {}
  virtual void on_event(command cmd) = 0;
};

// struct to hold information that we need when the response to a
// command is received. See client_base.hpp for more details on the
// required fields in this struct.
struct response_id {
   // The type of the adapter that should be used to deserialize the
   // response.
   using adapter_type = response_traits<std::vector<node>>::adapter_type;

   // The type of the session pointer.
   using session_ptr = std::weak_ptr<user_session_base>;

   // The redis command that was send in the request.
   command cmd = command::unknown;

   // The adapter.
   adapter_type adapter;

   // The pointer to the session the request belongs to.
   session_ptr session = std::shared_ptr<user_session_base>{nullptr};

   // Required from client_base.hpp.
   auto get_command() const noexcept { return cmd; }
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

   void start()
   {
     response_id id{command::ping, adapt(resp_), shared_from_this()};

     auto filler = [id](auto& req, auto const& msg)
        { req.push(id, msg);};

     co_spawn(socket_.get_executor(),
         [self = shared_from_this(), filler]{ return self->reader(filler); },
         net::detached);

     co_spawn(socket_.get_executor(),
         [self = shared_from_this()]{ return self->writer(); },
         net::detached);
   }

   void on_event(command cmd) override
   {
      assert(cmd == command::ping);
      deliver(resp_.back().data);
      resp_.clear();
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

   void deliver(const std::string& msg)
   {
     write_msgs_.push_back(msg);
     timer_.cancel_one();
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
   std::vector<node> resp_;
};
