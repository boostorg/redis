/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/read.hpp>

namespace aedis {

bool queue_pop(request_queue& reqs)
{
   assert(!std::empty(reqs));
   assert(!std::empty(reqs.front().req.cmds));

   reqs.front().req.cmds.pop();
   if (std::empty(reqs.front().req.cmds)) {
      reqs.pop();
      return true;
   }

   return false;
}

} // aedis
