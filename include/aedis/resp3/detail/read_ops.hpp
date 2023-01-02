/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_RESP3_READ_OPS_HPP
#define AEDIS_RESP3_READ_OPS_HPP

#include <aedis/resp3/detail/parser.hpp>

#include <boost/assert.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/core/ignore_unused.hpp>

#include <string_view>

namespace aedis::detail
{
template <class T>
auto is_cancelled(T const& self)
{
   return self.get_cancellation_state().cancelled() != boost::asio::cancellation_type_t::none;
}
}

#define AEDIS_CHECK_OP0(X)\
   if (ec || aedis::detail::is_cancelled(self)) {\
      X\
      self.complete(!!ec ? ec : boost::asio::error::operation_aborted);\
      return;\
   }

#define AEDIS_CHECK_OP1(X)\
   if (ec || aedis::detail::is_cancelled(self)) {\
      X\
      self.complete(!!ec ? ec : boost::asio::error::operation_aborted, {});\
      return;\
   }

namespace aedis::resp3::detail {

struct ignore_response {
   void operator()(node<std::string_view> nd, boost::system::error_code& ec)
   {
      switch (nd.data_type) {
         case resp3::type::simple_error: ec = error::resp3_simple_error; return;
         case resp3::type::blob_error: ec = error::resp3_blob_error; return;
         default: return;
      }
   }
};

template <
   class AsyncReadStream,
   class DynamicBuffer,
   class ResponseAdapter>
class parse_op {
private:
   AsyncReadStream& stream_;
   DynamicBuffer buf_;
   parser parser_;
   ResponseAdapter adapter_;
   std::size_t consumed_ = 0;
   std::size_t buffer_size_ = 0;
   boost::asio::coroutine coro_{};

public:
   parse_op(AsyncReadStream& stream, DynamicBuffer buf, ResponseAdapter adapter)
   : stream_ {stream}
   , buf_ {std::move(buf)}
   , adapter_ {std::move(adapter)}
   { }

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , std::size_t n = 0)
   {
      BOOST_ASIO_CORO_REENTER (coro_) for (;;) {
         if (!parser_.bulk_expected()) {
            BOOST_ASIO_CORO_YIELD
            boost::asio::async_read_until(stream_, buf_, "\r\n", std::move(self));
            AEDIS_CHECK_OP1(;);
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

               BOOST_ASIO_CORO_YIELD
               boost::asio::async_read(
                  stream_,
                  buf_.data(buffer_size_, parser_.bulk_length() + 2 - buffer_size_),
                  boost::asio::transfer_all(),
                  std::move(self));
               AEDIS_CHECK_OP1(;);
            }

            n = parser_.bulk_length() + 2;
            BOOST_ASSERT(buf_.size() >= n);
         }

         auto const res = parser_.consume(static_cast<char const*>(buf_.data(0, n).data()), n, ec);
         if (ec)
            return self.complete(ec, 0);

         if (!parser_.bulk_expected()) {
            adapter_(res.first, ec);
            if (ec)
               return self.complete(ec, 0);
         }

         buf_.consume(res.second);
         consumed_ += res.second;
         if (parser_.done()) {
            self.complete({}, consumed_);
            return;
         }
      }
   }
};

} // aedis::resp3::detail

#endif // AEDIS_RESP3_READ_OPS_HPP
