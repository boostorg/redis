/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <iostream>

#include <aedis/net.hpp>

#include <aedis/resp3/write.hpp>
#include <aedis/resp3/request.hpp>
#include <aedis/resp3/type.hpp>
#include <aedis/resp3/response.hpp>
#include <aedis/resp3/detail/parser.hpp>
#include <aedis/resp3/response_adapter_base.hpp>

#include <boost/asio/yield.hpp>

namespace aedis { namespace resp3 {

// The parser supports up to 5 levels of nested structures. The first
// element in the sizes stack is a sentinel and must be different from
// 1.
template <class AsyncReadStream, class Storage>
class parse_op {
private:
   AsyncReadStream& stream_;
   Storage* buf_ = nullptr;
   detail::parser parser_;
   int start_ = 1;

public:
   parse_op(AsyncReadStream& stream, Storage* buf, response_adapter_base* res)
   : stream_ {stream}
   , buf_ {buf}
   , parser_ {res}
   { }

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , std::size_t n = 0)
   {
      switch (start_) {
         for (;;) {
            if (parser_.bulk() == detail::parser::bulk_type::none) {
               case 1:
               start_ = 0;
               net::async_read_until(
                  stream_,
                  net::dynamic_buffer(*buf_),
                  "\r\n",
                  std::move(self));

               return;
            }

	    // On a bulk read we can't read until delimiter since the
	    // payload may contain the delimiter itself so we have to
	    // read the whole chunk. However if the bulk blob is small
	    // enough it may be already on the buffer buf_ we read
	    // last time. If it is, there is no need of initiating
	    // another async op otherwise we have to read the
	    // missing bytes.
            if (std::ssize(*buf_) < (parser_.bulk_length() + 2)) {
               start_ = 0;
	       auto const s = std::ssize(*buf_);
	       auto const l = parser_.bulk_length();
	       auto const to_read = static_cast<std::size_t>(l + 2 - s);
               buf_->resize(l + 2);
               net::async_read(
                  stream_,
                  net::buffer(buf_->data() + s, to_read),
                  net::transfer_all(),
                  std::move(self));
               return;
            }

            default:
	    {
	       if (ec)
		  return self.complete(ec);

	       n = parser_.advance(buf_->data(), n);
	       buf_->erase(0, n);
	       if (parser_.done())
		  return self.complete({});
	    }
         }
      }
   }
};

template <class SyncReadStream, class Storage>
auto read(
   SyncReadStream& stream,
   Storage& buf,
   response_adapter_base& res,
   boost::system::error_code& ec)
{
   detail::parser p {&res};
   std::size_t n = 0;
   do {
      if (p.bulk() == detail::parser::bulk_type::none) {
	 n = net::read_until(stream, net::dynamic_buffer(buf), "\r\n", ec);
	 if (ec || n < 3)
	    return n;
      } else {
	 auto const s = std::ssize(buf);
	 auto const l = p.bulk_length();
	 if (s < (l + 2)) {
	    buf.resize(l + 2);
	    auto const to_read = static_cast<std::size_t>(l + 2 - s);
	    n = net::read(stream, net::buffer(buf.data() + s, to_read));
	    assert(n >= to_read);
	    if (ec)
	       return n;
	 }
      }

      n = p.advance(buf.data(), n);
      buf.erase(0, n);
   } while (!p.done());

   return n;
}

template<class SyncReadStream, class Storage>
std::size_t
read(
   SyncReadStream& stream,
   Storage& buf,
   response_adapter_base& res)
{
   boost::system::error_code ec;
   auto const n = read(stream, buf, res, ec);

   if (ec)
       BOOST_THROW_EXCEPTION(boost::system::system_error{ec});

   return n;
}

template <
   class AsyncReadStream,
   class Storage,
   class CompletionToken =
      net::default_completion_token_t<typename AsyncReadStream::executor_type>
   >
auto async_read_one(
   AsyncReadStream& stream,
   Storage& buffer,
   response_adapter_base& res,
   CompletionToken&& token =
      net::default_completion_token_t<typename AsyncReadStream::executor_type>{})
{
   return net::async_compose
      < CompletionToken
      , void(boost::system::error_code)
      >(parse_op<AsyncReadStream, Storage> {stream, &buffer, &res},
        token,
        stream);
}

template <class AsyncReadStream, class Storage>
class type_op {
private:
   AsyncReadStream& stream_;
   Storage* buf_ = nullptr;

public:
   type_op(AsyncReadStream& stream, Storage* buf)
   : stream_ {stream}
   , buf_ {buf}
   {
      assert(buf_);
   }

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , std::size_t n = 0)
   {
      if (ec) {
	 self.complete(ec, type::invalid);
         return;
      }

      if (std::empty(*buf_)) {
	 net::async_read_until(
	    stream_,
	    net::dynamic_buffer(*buf_),
	    "\r\n",
	    std::move(self));
	 return;
      }

      assert(!std::empty(*buf_));
      auto const type = to_type(buf_->front());
      // TODO: when type = type::invalid should we report an error or
      // complete normally and let the caller check whether it is invalid.
      self.complete(ec, type);
      return;
   }
};

template <
   class AsyncReadStream,
   class Storage,
   class CompletionToken =
      net::default_completion_token_t<typename AsyncReadStream::executor_type>
   >
auto async_read_type(
   AsyncReadStream& stream,
   Storage& buffer,
   CompletionToken&& token =
      net::default_completion_token_t<typename AsyncReadStream::executor_type>{})
{
   return net::async_compose
      < CompletionToken
      , void(boost::system::error_code, type)
      >(type_op<AsyncReadStream, Storage> {stream, &buffer}, token, stream);
}

struct consumer_op {
   net::ip::tcp::socket& socket;
   std::string& buffer;
   std::queue<request>& requests;
   response& resp;
   type& m_type;
   net::coroutine& coro;

   template <class Self>
   void operator()(
      Self& self,
      boost::system::error_code const& ec = {},
      type t = type::invalid)
   {
      reenter (coro) for (;;)
      {
         yield async_write_some(socket, requests, std::move(self));
         if (ec) {
            self.complete(ec, type::invalid);
            return;
         }

         do {
            do {
               yield async_read_type(socket, buffer, std::move(self));
               if (ec) {
                  self.complete(ec, type::invalid);
                  return;
               }

               m_type = t;

               yield
               {
                  if (m_type == type::push) {
		     auto* adapter = resp.select_adapter(m_type, command::unknown, {});
		     async_read_one(socket, buffer, *adapter, std::move(self));
		  } else {
		     auto const& pair = requests.front().ids.front();
		     auto* adapter = resp.select_adapter(m_type, pair.first, pair.second);
		     async_read_one(socket, buffer, *adapter, std::move(self));
		  }
               }

               if (ec) {
                  self.complete(ec, type::invalid);
                  return;
               }

               yield self.complete(ec, m_type);

               if (m_type != type::push)
                  requests.front().ids.pop();

            } while (!std::empty(requests.front().ids));
            requests.pop();
         } while (std::empty(requests));
      }
   }
};

struct consumer {
   std::string buffer;
   response resp;
   net::coroutine coro = net::coroutine();
   type t = type::invalid;

   template<class CompletionToken>
   auto async_consume(
      net::ip::tcp::socket& socket,
      std::queue<request>& requests,
      response& resp,
      CompletionToken&& token)
   {
     return net::async_compose<
	CompletionToken,
	void(boost::system::error_code, type)>(
	   consumer_op{socket, buffer, requests, resp, t, coro}, token, socket);
   }
};

} // resp3
} // aedis

#include <boost/asio/unyield.hpp>
