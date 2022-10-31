/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_RESP3_PARSER_HPP
#define AEDIS_RESP3_PARSER_HPP

#include <array>
#include <limits>
#include <system_error>

#include <boost/assert.hpp>
#include <boost/utility/string_view.hpp>

#include <aedis/error.hpp>
#include <aedis/resp3/node.hpp>

namespace aedis::resp3::detail {

auto parse_uint(char const* data, std::size_t size, boost::system::error_code& ec) -> std::size_t;

template <class ResponseAdapter>
class parser {
private:
   using node_type = node<boost::string_view>;
   static constexpr std::size_t max_embedded_depth = 5;

   ResponseAdapter adapter_;

   // The current depth. Simple data types will have depth 0, whereas
   // the elements of aggregates will have depth 1. Embedded types
   // will have increasing depth.
   std::size_t depth_ = 0;

   // The parser supports up to 5 levels of nested structures. The
   // first element in the sizes stack is a sentinel and must be
   // different from 1.
   std::array<std::size_t, max_embedded_depth + 1> sizes_ = {{1}};

   // Contains the length expected in the next bulk read.
   std::size_t bulk_length_ = (std::numeric_limits<std::size_t>::max)();

   // The type of the next bulk. Contains type::invalid if no bulk is
   // expected.
   type bulk_ = type::invalid;

public:
   explicit parser(ResponseAdapter adapter)
   : adapter_{adapter}
   {
      sizes_[0] = 2; // The sentinel must be more than 1.
   }

   // Returns the number of bytes that have been consumed.
   auto
   consume(char const* data, std::size_t n, boost::system::error_code& ec) -> std::size_t
   {
      if (bulk_ != type::invalid) {
         n = bulk_length_ + 2;
         switch (bulk_) {
            case type::streamed_string_part:
            {
               BOOST_ASSERT(bulk_length_ != 0);
               adapter_({bulk_, 1, depth_, {data, bulk_length_}}, ec);
               if (ec)
                  return 0;
            } break;
            default:
	    {
	       adapter_({bulk_, 1, depth_, {data, bulk_length_}}, ec);
	       if (ec)
		  return 0;
	    }
         }

         bulk_ = type::invalid;
         --sizes_[depth_];

      } else if (sizes_[depth_] != 0) {
         auto const t = to_type(*data);
         switch (t) {
            case type::streamed_string_part:
            {
               bulk_length_ = parse_uint(data + 1, n - 2, ec);
	       if (ec)
		  return 0;

               if (bulk_length_ == 0) {
                  adapter_({type::streamed_string_part, 1, depth_, {}}, ec);
                  sizes_[depth_] = 0; // We are done.
               } else {
                  bulk_ = type::streamed_string_part;
               }
            } break;
            case type::blob_error:
            case type::verbatim_string:
            case type::blob_string:
            {
               if (data[1] == '?') {
                  // NOTE: This can only be triggered with blob_string.
		  // Trick: A streamed string is read as an aggregate
		  // of infinite lenght. When the streaming is done
		  // the server is supposed to send a part with length
		  // 0.
                  sizes_[++depth_] = (std::numeric_limits<std::size_t>::max)();
               } else {
                  bulk_length_ = parse_uint(data + 1, n - 2, ec);
                  if (ec)
                     return 0;

                  bulk_ = t;
               }
            } break;
            case type::boolean:
            {
               if (n == 3) {
                   ec = error::empty_field;
                   return 0;
               }

               if (data[1] != 'f' && data[1] != 't') {
                   ec = error::unexpected_bool_value;
                   return 0;
               }

               adapter_({t, 1, depth_, {data + 1, n - 3}}, ec);
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

               adapter_({t, 1, depth_, {data + 1, n - 3}}, ec);
	       if (ec)
		  return 0;

               --sizes_[depth_];
            } break;
            case type::simple_error:
            case type::simple_string:
            {
               adapter_({t, 1, depth_, {&data[1], n - 3}}, ec);
	       if (ec)
		  return 0;

               --sizes_[depth_];
            } break;
            case type::null:
            {
               adapter_({type::null, 1, depth_, {}}, ec);
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
	       auto const l = parse_uint(data + 1, n - 2, ec);
               if (ec)
                  return 0;

               adapter_({t, l, depth_, {}}, ec);
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
	       ec = error::invalid_data_type;
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
   [[nodiscard]] auto done() const noexcept
      { return depth_ == 0 && bulk_ == type::invalid; }

   // The bulk type expected in the next read. If none is expected returns
   // type::invalid.
   [[nodiscard]] auto bulk() const noexcept { return bulk_; }

   // The length expected in the the next bulk.
   [[nodiscard]] auto bulk_length() const noexcept { return bulk_length_; }
};

} // detail::resp3::aedis

#endif // AEDIS_RESP3_PARSER_HPP
