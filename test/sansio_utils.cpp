/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include "sansio_utils.hpp"

#include <boost/redis/detail/multiplexer.hpp>
#include <boost/core/ignore_unused.hpp>

namespace boost::redis::detail {

void read(multiplexer& mpx, std::string_view data)
{
   auto const ec = mpx.prepare_read();
   ignore_unused(ec);
   BOOST_ASSERT(ec == system::error_code{});
   auto const buffer = mpx.get_prepared_read_buffer();
   BOOST_ASSERT(buffer.size() >= data.size());
   std::copy(data.cbegin(), data.cend(), buffer.begin());
   mpx.commit_read(data.size());
}

}  // namespace boost::redis::detail
