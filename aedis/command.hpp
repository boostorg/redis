/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_COMMAND_HPP
#define AEDIS_COMMAND_HPP

#include <boost/utility/string_view.hpp>

namespace aedis {

bool has_push_response(boost::string_view cmd);

} // aedis

#endif // AEDIS_COMMAND_HPP
