pkg_name = aedis
pkg_version = 1.0.0
pkg_revision = 1
tarball_name = $(pkg_name)-$(pkg_version)-$(pkg_revision)
tarball_dir = $(pkg_name)-$(pkg_version)
prefix = /opt/$(pkg_name)-$(pkg_version)
incdir = $(prefix)/include/$(pkg_name)

CXX = /opt/gcc-10.2.0/bin/g++-10.2.0

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
examples += async_basic
examples += async_reconnect
examples += async_all_hashes
examples += async_events
examples += async_pubsub

tests =
tests += general

remove =
remove += $(examples)
remove += $(tests)
remove += $(addsuffix .o, $(examples))
remove += $(addsuffix .o, $(tests))
remove += Makefile.dep
remove += $(tarball_name).tar.gz

all: $(tests) $(examples)

Makefile.dep:
	-$(CXX) -MM -I./include ./examples/*.cpp ./tests/*.cpp > $@

-include Makefile.dep

$(examples): % : examples/%.cpp
	$(CXX) -o $@ $< $(CPPFLAGS) $(LDFLAGS)

$(tests): % : tests/%.cpp
	$(CXX) -o $@ $< $(CPPFLAGS) $(LDFLAGS)

.PHONY: check
check: $(tests)
	./general

.PHONY: install
install:
	install --mode=444 -D include/aedis/* --target-directory $(incdir)

uninstall:
	rm -rf $(incdir)

.PHONY: clean
clean:
	rm -f $(remove)

$(tarball_name).tar.gz:
	git archive --format=tar.gz --prefix=$(tarball_dir)/ HEAD > $(tarball_name).tar.gz

.PHONY: dist
dist: $(tarball_name).tar.gz

