/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/net.hpp>

#include <aedis/resp3/request.hpp>
#include <aedis/resp3/type.hpp>
#include <aedis/resp3/response.hpp>
#include <aedis/resp3/response_adapter_base.hpp>
#include <aedis/resp3/detail/parser.hpp>
#include <aedis/resp3/detail/write.hpp>

#include <boost/asio/yield.hpp>

namespace aedis {
namespace resp3 {
namespace detail {

template <class SyncReadStream, class Storage>
auto read(
   SyncReadStream& stream,
   Storage& buf,
   response_adapter_base& res,
   boost::system::error_code& ec)
{
   parser p {&res};
   std::size_t n = 0;
   do {
      if (p.bulk() == parser::bulk_type::none) {
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

type to_type(char c)
{
   switch (c) {
      case '!': return type::blob_error;
      case '=': return type::verbatim_string;
      case '$': return type::blob_string;
      case ';': return type::streamed_string_part;
      case '-': return type::simple_error;
      case ':': return type::number;
      case ',': return type::doublean;
      case '#': return type::boolean;
      case '(': return type::big_number;
      case '+': return type::simple_string;
      case '_': return type::null;
      case '>': return type::push;
      case '~': return type::set;
      case '*': return type::array;
      case '|': return type::attribute;
      case '%': return type::map;
      default: return type::invalid;
   }
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

template <class AsyncReadWriteStream>
struct consumer_op {
   AsyncReadWriteStream& stream;
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
	 // Writes the next request in the queue and possibly some
	 // more if they contain only push types as response.
         yield async_write_some(stream, requests, std::move(self));
         if (ec) {
            self.complete(ec, type::invalid);
            return;
         }

	 // Loops on a read while there is nothing to write.
         do {
	    // Loops until a response to each of the commands in the
	    // pipeline has been received.
            do {
               yield async_read_type(stream, buffer, std::move(self));
               if (ec) {
                  self.complete(ec, type::invalid);
                  return;
               }

               m_type = t;

               yield
               {
                  if (m_type == type::push) {
		     auto* adapter = resp.select_adapter(m_type);
		     async_read_one(stream, buffer, *adapter, std::move(self));
		  } else {
		     auto const cmd = requests.front().commands.front();
		     auto* adapter = resp.select_adapter(m_type, cmd);
		     async_read_one(stream, buffer, *adapter, std::move(self));
		  }
               }

               if (ec) {
                  self.complete(ec, type::invalid);
                  return;
               }

               yield self.complete(ec, m_type);

               if (m_type != type::push)
		  requests.front().commands.pop();

            } while (!std::empty(requests) && !std::empty(requests.front().commands));

	    if (!std::empty(requests))
	       requests.pop();

         } while (std::empty(requests));
      }
   }
};

} // detail
} // resp3
} // aedis

#include <boost/asio/unyield.hpp>
