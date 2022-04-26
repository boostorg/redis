/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_RESP3_READ_OPS_HPP
#define AEDIS_RESP3_READ_OPS_HPP

#include <boost/assert.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/utility/string_view.hpp>

#include <aedis/resp3/detail/parser.hpp>

namespace aedis {
namespace resp3 {
namespace detail {

#include <boost/asio/yield.hpp>

struct ignore_response {
   void operator()(node<boost::string_view>, boost::system::error_code&) { }
};

template <
   class AsyncReadStream,
   class DynamicBuffer,
   class ResponseAdapter>
class parse_op {
private:
   AsyncReadStream& stream_;
   DynamicBuffer buf_;
   parser<ResponseAdapter> parser_;
   std::size_t consumed_;
   std::size_t buffer_size_;
   boost::asio::coroutine coro_;

public:
   parse_op(AsyncReadStream& stream, DynamicBuffer buf, ResponseAdapter adapter)
   : stream_ {stream}
   , buf_ {buf}
   , parser_ {adapter}
   , consumed_{0}
   { }

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , std::size_t n = 0)
   {
      reenter (coro_) for (;;) {
         if (parser_.bulk() == type::invalid) {
            yield
            boost::asio::async_read_until(stream_, buf_, "\r\n", std::move(self));

            if (ec) {
               self.complete(ec, 0);
               return;
            }
         } else {
	    // On a bulk read we can't read until delimiter since the
	    // payload may contain the delimiter itself so we have to
	    // read the whole chunk. However if the bulk blob is small
	    // enough it may be already on the buffer (from the last
	    // read), in which case there is no need of initiating
	    // another async op, otherwise we have to read the missing
	    // bytes.
            if (buf_.size() < (parser_.bulk_length() + 2)) {
               buffer_size_ = buf_.size();
               buf_.grow(parser_.bulk_length() + 2 - buffer_size_);

               yield
               boost::asio::async_read(
                  stream_,
                  buf_.data(buffer_size_, parser_.bulk_length() + 2 - buffer_size_),
                  boost::asio::transfer_all(),
                  std::move(self));

               if (ec) {
                  self.complete(ec, 0);
                  return;
               }
            }

            n = parser_.bulk_length() + 2;
            BOOST_ASSERT(buf_.size() >= n);
         }

         n = parser_.consume((char const*)buf_.data(0, n).data(), n, ec);
         if (ec) {
            self.complete(ec, 0);
            return;
         }

         buf_.consume(n);
         consumed_ += n;
         if (parser_.done()) {
            self.complete({}, consumed_);
            return;
         }
      }
   }
};

#include <boost/asio/unyield.hpp>

} // detail
} // resp3
} // aedis

#endif // AEDIS_RESP3_READ_OPS_HPP
