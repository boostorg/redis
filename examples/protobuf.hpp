/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_PROTOBUF_HPP
#define BOOST_REDIS_PROTOBUF_HPP

#include <boost/redis/resp3/serialization.hpp>
#include <boost/system/errc.hpp>

namespace boost::redis::protobuf
{

// Below I am using a Boost.Redis to indicate a protobuf error, this
// is ok for an example, users however might want to define their own
// error codes.

template <class T>
void to_bulk(std::string& to, T const& u)
{
   std::string tmp;
   if (!u.SerializeToString(&tmp))
      throw system::system_error(redis::error::invalid_data_type);

   boost::redis::resp3::boost_redis_to_bulk(to, tmp);
}

template <class T>
void from_bulk(T& u, std::string_view sv, system::error_code& ec)
{
   std::string const tmp {sv};
   if (!u.ParseFromString(tmp))
      ec = redis::error::invalid_data_type;
}

} // boost::redis::json

#endif // BOOST_REDIS_PROTOBUF_HPP
