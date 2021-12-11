/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <string_view>
#include <aedis/net.hpp>

namespace aedis {
namespace resp3 {
namespace detail {

type to_type(char c);

// Converts a decimal number in ascii format to an integer.
std::size_t length(char const* p);

template <class ResponseAdapter>
class parser {
private:
   ResponseAdapter adapter_;
   std::size_t depth_;
   std::size_t sizes_[6];
   std::size_t bulk_length_;
   type bulk_;

   void init()
   {
      depth_ = 0;
      sizes_[0] = 2;
      sizes_[1] = 1;
      sizes_[2] = 1;
      sizes_[3] = 1;
      sizes_[4] = 1;
      sizes_[5] = 1;
      sizes_[6] = 1;
      bulk_ = type::invalid;
      bulk_length_ = std::numeric_limits<std::size_t>::max();
   }

public:
   parser(ResponseAdapter adapter)
   : adapter_{adapter}
   { init(); }

   // Returns the number of bytes in data that have been consumed.
   std::size_t advance(char const* data, std::size_t n)
   {
      if (bulk_ != type::invalid) {
         n = bulk_length_ + 2;
         switch (bulk_) {
            case type::streamed_string_part:
            {
              if (bulk_length_ == 0) {
                 sizes_[depth_] = 1;
              } else {
                 adapter_(bulk_, 1, depth_, data, bulk_length_);
              }
            } break;
            default: adapter_(bulk_, 1, depth_, data, bulk_length_);
         }

         bulk_ = type::invalid;
         --sizes_[depth_];

      } else if (sizes_[depth_] != 0) {
         auto const t = to_type(*data);
         switch (t) {
            case type::blob_error:
            case type::verbatim_string:
            case type::streamed_string_part:
            {
               bulk_length_ = length(data + 1);
               bulk_ = t;
            } break;
            case type::blob_string:
            {
               if (*(data + 1) == '?') {
                  sizes_[++depth_] = std::numeric_limits<std::size_t>::max();
               } else {
                  bulk_length_ = length(data + 1);
                  bulk_ = type::blob_string;
               }
            } break;
            case type::simple_error:
            case type::number:
            case type::doublean:
            case type::boolean:
            case type::big_number:
            case type::simple_string:
            {
               adapter_(t, 1, depth_, data + 1, n - 3);
               --sizes_[depth_];
            } break;
            case type::null:
            {
               adapter_(type::null, 1, depth_, nullptr, 0);
               --sizes_[depth_];
            } break;
            case type::push:
            case type::set:
            case type::array:
            case type::attribute:
            case type::map:
            {
               auto const l = length(data + 1);
               adapter_(t, l, depth_, nullptr, 0);

               if (l == 0) {
                  --sizes_[depth_];
               } else {
                  auto const m = element_multiplicity(t);
                  sizes_[++depth_] = m * l;
               }
            } break;
            default:
            {
               // TODO: This should cause an error not an assert.
               assert(false);
            }
         }
      }
      
      while (sizes_[depth_] == 0)
         --sizes_[--depth_];
      
      return n;
   }


   // returns true when the parser is done with the current message.
   auto done() const noexcept
      { return depth_ == 0 && bulk_ == type::invalid; }

   // The bulk type expected in the next read. If none is expected returns
   // type::invalid.
   auto bulk() const noexcept { return bulk_; }

   // The lenght of the next expected bulk_length.
   auto bulk_length() const noexcept { return bulk_length_; }
};

// The parser supports up to 5 levels of nested structures. The first
// element in the sizes stack is a sentinel and must be different from
// 1.
template <
   class AsyncReadStream,
   class Storage,
   class ResponseAdapter>
class parse_op {
private:
   AsyncReadStream& stream_;
   Storage* buf_ = nullptr;
   parser<ResponseAdapter> parser_;
   int start_ = 1;

public:
   parse_op(AsyncReadStream& stream, Storage* buf, ResponseAdapter adapter)
   : stream_ {stream}
   , buf_ {buf}
   , parser_ {adapter}
   { }

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , std::size_t n = 0)
   {
      switch (start_) {
         for (;;) {
            if (parser_.bulk() == type::invalid) {
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

} // detail
} // resp3
} // aedis
