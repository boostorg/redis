
#CXX = /opt/gcc-10.2.0/bin/g++-10.2.0

CPPFLAGS =
CPPFLAGS += -g
CPPFLAGS += -O0
CPPFLAGS += -std=c++20 -Wall #-Werror
CPPFLAGS += -fcoroutines
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
examples += async_all_hashes

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

