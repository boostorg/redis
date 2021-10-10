/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <chrono>

#include <aedis/net.hpp>
#include <aedis/resp3/request.hpp>

#include <boost/beast/core/stream_traits.hpp>
#include <boost/asio/yield.hpp>

namespace aedis {
namespace resp3 {

/** Prepares the back of a queue to receive further commands and
 *  returns true if a write is possible.
 */
bool prepare_next(std::queue<request>& reqs);

} // resp3
} // aedis

#include <boost/asio/unyield.hpp>
