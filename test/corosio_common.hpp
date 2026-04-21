//
// Copyright (c) 2026 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_TEST_COROSIO_COMMON_HPP
#define BOOST_REDIS_TEST_COROSIO_COMMON_HPP

#include <boost/assert/source_location.hpp>
#include <boost/capy/task.hpp>

namespace boost::redis::test {

void run_coroutine_test(capy::task<void> test, source_location loc = BOOST_CURRENT_LOCATION);

}

#endif
