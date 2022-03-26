/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <string_view>
#include <charconv>
#include <system_error>
#include <limits>

#include <aedis/resp3/error.hpp>

namespace aedis {
namespace resp3 {
namespace detail {

// Converts a wire-format RESP3 type (char) to a resp3 type.
type to_type(char c);

template <class ResponseAdapter>
class parser {
private:
   static constexpr std::size_t max_embedded_depth = 5;

   ResponseAdapter adapter_;

   // The current depth. Simple data types will have depth 0, whereas
   // the elements of aggregates will have depth 1. Embedded types
   // will have increasing depth.
   std::size_t depth_ = 0;

   // The parser supports up to 5 levels of nested structures. The
   // first element in the sizes stack is a sentinel and must be
   // different from 1.
   std::size_t sizes_[max_embedded_depth + 1] = {1};

   // Contains the length expected in the next bulk read.
   std::size_t bulk_length_ = (std::numeric_limits<std::size_t>::max)();

   // The type of the next bulk. Contains type::invalid if no bulk is
   // expected.
   type bulk_ = type::invalid;

public:
   parser(ResponseAdapter adapter)
   : adapter_{adapter}
   {
      sizes_[0] = 2; // The sentinel must be more than 1.
   }

   // Returns the number of bytes that have been consumed.
   std::size_t
   advance(char const* data, std::size_t n, boost::system::error_code& ec)
   {
      if (bulk_ != type::invalid) {
         n = bulk_length_ + 2;
         switch (bulk_) {
            case type::streamed_string_part:
            {
              if (bulk_length_ == 0) {
                 sizes_[depth_] = 1;
              } else {
                 adapter_(bulk_, 1, depth_, data, bulk_length_, ec);
		 if (ec)
		    return 0;
              }
            } break;
            default:
	    {
	       adapter_(bulk_, 1, depth_, data, bulk_length_, ec);
	       if (ec)
		  return 0;
	    }
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
               auto const r =
		  std::from_chars(data + 1, data + n - 2, bulk_length_);
	       if (r.ec != std::errc()) {
		  ec = error::not_a_number;
		  return 0;
	       }

               bulk_ = t;
            } break;
            case type::blob_string:
            {
               if (*(data + 1) == '?') {
		  // Trick: A streamed string is read as an aggregate
		  // of infinite lenght. When the streaming is done
		  // the server is supposed to send a part with lenght
		  // 0.
                  sizes_[++depth_] = (std::numeric_limits<std::size_t>::max)();
               } else {
		  auto const r =
		     std::from_chars(data + 1, data + n - 2, bulk_length_);
		  if (r.ec != std::errc()) {
		     ec = error::not_a_number;
		     return 0;
		  }

                  bulk_ = type::blob_string;
               }
            } break;
            case type::boolean:
            {
               if (n == 3) {
                   ec = error::empty_field;
                   return 0;
               }

               if (*(data + 1) != 'f' && *(data + 1) != 't') {
                   ec = error::unexpected_bool_value;
                   return 0;
               }

               adapter_(t, 1, depth_, data + 1, n - 3, ec);
	       if (ec)
		  return 0;

               --sizes_[depth_];
            } break;
            case type::doublean:
            case type::big_number:
            case type::number:
            {
               if (n == 3) {
                   ec = error::empty_field;
                   return 0;
               }

               adapter_(t, 1, depth_, data + 1, n - 3, ec);
	       if (ec)
		  return 0;

               --sizes_[depth_];
            } break;
            case type::simple_error:
            case type::simple_string:
            {
               adapter_(t, 1, depth_, data + 1, n - 3, ec);
	       if (ec)
		  return 0;

               --sizes_[depth_];
            } break;
            case type::null:
            {
               adapter_(type::null, 1, depth_, nullptr, 0, ec);
	       if (ec)
		  return 0;

               --sizes_[depth_];
            } break;
            case type::push:
            case type::set:
            case type::array:
            case type::attribute:
            case type::map:
            {
	       std::size_t l;
               auto const r = std::from_chars(data + 1, data + n - 2, l);
	       if (r.ec != std::errc()) {
		  ec = error::not_a_number;
		  return 0;
	       }

               adapter_(t, l, depth_, nullptr, 0, ec);
	       if (ec)
		  return 0;

               if (l == 0) {
                  --sizes_[depth_];
               } else {
		  if (depth_ == max_embedded_depth) {
		     ec = error::exceeeds_max_nested_depth;
		     return 0;
		  }

                  ++depth_;

                  sizes_[depth_] = l * element_multiplicity(t);
               }
            } break;
            default:
            {
	       ec = error::invalid_type;
	       return 0;
            }
         }
      }
      
      while (sizes_[depth_] == 0) {
         --depth_;
         --sizes_[depth_];
      }
      
      return n;
   }

   // Returns true when the parser is done with the current message.
   auto done() const noexcept
      { return depth_ == 0 && bulk_ == type::invalid; }

   // The bulk type expected in the next read. If none is expected returns
   // type::invalid.
   auto bulk() const noexcept { return bulk_; }

   // The length expected in the the next bulk.
   auto bulk_length() const noexcept { return bulk_length_; }
};

} // detail
} // resp3
} // aedis
