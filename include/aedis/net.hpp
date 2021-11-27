/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <boost/asio.hpp>

/** \file net.hpp
 *  \brief The network library used in redis.
 *
 *  At the moment only boost ASIO is supported. In the future we may add support to standalone ASIO.
 */

namespace aedis {
namespace net = boost::asio;
}

