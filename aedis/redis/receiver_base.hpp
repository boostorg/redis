/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/generic/receiver_base.hpp>
#include <aedis/redis/command.hpp>

namespace aedis {
namespace redis {

/** @brief Convenience typedef for the Redis receiver_base.
 *  @ingroup any
 */
template <class ...Ts>
using receiver_base = generic::receiver_base<command, Ts...>;

} // redis
} // aedis
