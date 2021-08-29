/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/write.hpp>

namespace aedis {

bool prepare_queue(std::queue<pipeline>& reqs)
{
   if (std::empty(reqs)) {
      reqs.push({});
      return true;
   }

   if (reqs.back().sent) {
      reqs.push({});
      return false;
   }

   return false;
}

} // aedis

