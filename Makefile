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

all: example general

Makefile.dep:
	-$(CXX) -MM -I./include ./examples/*.cpp ./tests/*.cpp > $@

-include Makefile.dep

example: examples/example.cpp
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LDFLAGS)

general: % : tests/general.cpp
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LDFLAGS)

.PHONY: check
check: general
	./general

.PHONY: clean
clean:
	rm -f example example.o general general.o Makefile.dep

