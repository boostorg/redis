//
// Copyright (c) 2026 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/assert/source_location.hpp>
#include <boost/capy/ex/run_async.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/corosio/io_context.hpp>

#include "common.hpp"
#include "corosio_common.hpp"

#include <iostream>

void boost::redis::detail::run_coroutine_test(capy::task<void> test, source_location loc)
{
   // Set a timeout to the tests, so they don't hang on error
   bool finished = false;
   auto wrapper_fn = [test = std::move(test), &finished]() mutable -> capy::task<void> {
      co_await std::move(test);
      finished = true;
   };

   // Actually run the test
   corosio::io_context ctx;
   capy::run_async(ctx.get_executor())(wrapper_fn());
   ctx.run_for(test_timeout);

   // Check that it finished
   if (!BOOST_TEST(finished))
      std::cerr << "  Called from " << loc << std::endl;
}