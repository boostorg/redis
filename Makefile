# Copyright (c) 2019 - 2020 Marcelo Zimbres Silva (mzimbres at gmail dot com)
# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

CPPFLAGS =
CPPFLAGS +=  -g
CPPFLAGS += -I/opt/boost_1_74_0/include
CPPFLAGS += -D BOOST_ASIO_CONCURRENCY_HINT_1=BOOST_ASIO_CONCURRENCY_HINT_UNSAFE
CPPFLAGS += -D BOOST_ASIO_NO_DEPRECATED 
CPPFLAGS += -D BOOST_ASIO_NO_TS_EXECUTORS 

LDFLAGS += -pthread
LDFLAGS += -lfmt

all: examples tests

Makefile.dep:
	-$(CXX) -MM ./*.cpp > $@

-include Makefile.dep

examples: examples.cpp
	$(CXX) -o $@ $^ -std=c++20 $(CPPFLAGS) $(LDFLAGS) -fcoroutines

tests: % : tests.cpp
	$(CXX) -o $@ $^ -std=c++17 $(CPPFLAGS) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f examples examples.o tests tests.o Makefile.dep

