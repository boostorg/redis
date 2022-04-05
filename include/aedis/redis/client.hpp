/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/generic/client.hpp>
#include <aedis/redis/command.hpp>

namespace aedis {
namespace redis {

/** @brief Convenience typedef for redis clients.
 *  @ingroup any
 */
template <class AsyncReadWriteStream>
using client = generic::client<AsyncReadWriteStream, command>;

} // redis
} // aedis
