# Copyright (c) 2019 - 2020 Marcelo Zimbres Silva (mzimbres at gmail dot com)
# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

#CXX = /opt/gcc-10.2.0/bin/g++-10.2.0

CPPFLAGS =
CPPFLAGS +=  -g -O0
CPPFLAGS +=  -std=c++20
CPPFLAGS +=  -fcoroutines
CPPFLAGS += -I/opt/boost_1_74_0/include
CPPFLAGS += -I./include
CPPFLAGS += -D BOOST_ASIO_CONCURRENCY_HINT_1=BOOST_ASIO_CONCURRENCY_HINT_UNSAFE
CPPFLAGS += -D BOOST_ASIO_NO_DEPRECATED 
CPPFLAGS += -D BOOST_ASIO_NO_TS_EXECUTORS 

LDFLAGS += -pthread

examples =
examples += sync_basic
examples += sync_responses
examples += sync_events
examples += async_basic
examples += async_reconnect
examples += async_own_struct
examples += async

tests =
tests += general

remove =
remove += $(examples)
remove += $(tests)
remove += $(addsuffix .o, $(examples))
remove += $(addsuffix .o, $(tests))
remove += Makefile.dep

all: general $(examples)

Makefile.dep:
	-$(CXX) -MM -I./include ./examples/*.cpp ./tests/*.cpp > $@

-include Makefile.dep

$(examples): % : examples/%.cpp
	$(CXX) -o $@ $< $(CPPFLAGS) $(LDFLAGS)

$(tests): % : tests/%.cpp
	$(CXX) -o $@ $< $(CPPFLAGS) $(LDFLAGS)

.PHONY: check
check: general
	./general

.PHONY: clean
clean:
	rm -f $(remove)

