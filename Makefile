# Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres at gmail dot com)
# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

CPPFLAGS += -std=c++17 -g
CPPFLAGS += -I/opt/boost_1_71_0/include
CPPFLAGS += -DBOOST_ASIO_CONCURRENCY_HINT_1=BOOST_ASIO_CONCURRENCY_HINT_UNSAFE

all: examples

Makefile.dep:
	-$(CXX) -MM ./*.cpp > $@

-include Makefile.dep

examples: % : %.o
	$(CXX) -o $@ $^ $(CPPFLAGS) -lfmt -lpthread

.PHONY: clean
clean:
	rm -f examples examples.o Makefile.dep

