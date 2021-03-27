pkg_name = aedis
pkg_version = 1.0.0
pkg_revision = 1
tarball_name = $(pkg_name)-$(pkg_version)-$(pkg_revision)
tarball_dir = $(pkg_name)-$(pkg_version)
prefix = /opt/$(pkg_name)-$(pkg_version)
incdir = $(prefix)/include/$(pkg_name)
exec_prefix = $(prefix)
libdir = $(exec_prefix)/lib

VPATH = ./src/

#CXX = /opt/gcc-10.2.0/bin/g++-10.2.0
#CXX = clang++

CPPFLAGS =
CPPFLAGS += -std=c++20 -Wall #-Werror
#CPPFLAGS += -stdlib=libc++
CPPFLAGS += -fcoroutines
CPPFLAGS += -I/opt/boost_1_74_0/include
CPPFLAGS += -I./include
CPPFLAGS += -D BOOST_ASIO_CONCURRENCY_HINT_1=BOOST_ASIO_CONCURRENCY_HINT_UNSAFE
CPPFLAGS += -D BOOST_ASIO_NO_DEPRECATED 
CPPFLAGS += -D BOOST_ASIO_NO_TS_EXECUTORS 

CPPFLAGS += -g
CPPFLAGS += -fsanitize=address
CPPFLAGS += -O0

LDFLAGS += -pthread

examples =
examples += sync_basic
examples += async_basic
examples += async_low_level

tests =
tests += general

aedis_obj =
aedis_obj += aedis.o

remove =
remove += $(examples)
remove += $(aedis_obj)
remove += $(tests)
remove += $(addprefix examples/, $(addsuffix .o, $(examples)))
remove += $(addsuffix .o, $(tests))
remove += Makefile.dep
remove += $(tarball_name).tar.gz

.PHONY: all
all: $(tests) $(examples)

Makefile.dep:
	-$(CXX) -MM -I./include ./examples/*.cpp ./tests/*.cpp > $@

-include Makefile.dep

$(examples): % : examples/%.o $(aedis_obj)
	$(CXX) -o $@ $< $(CPPFLAGS) $(LDFLAGS) $(aedis_obj)

$(tests): % : tests/%.cpp $(aedis_obj)
	$(CXX) -o $@ $< $(CPPFLAGS) $(LDFLAGS) $(aedis_obj)

.PHONY: check
check: $(tests)
	./general

.PHONY: install
install:
	install --mode=444 -D include/aedis/*.hpp --target-directory $(incdir)
	install --mode=444 -D include/aedis/impl/*.hpp --target-directory $(incdir)/impl
	install --mode=444 -D include/aedis/impl/*.ipp --target-directory $(incdir)/impl
	install --mode=444 -D include/aedis/detail/*.hpp --target-directory $(incdir)/detail
	#install --mode=444 -D include/aedis/detail/impl/*.hpp --target-directory $(incdir)/detail/impl
	install --mode=444 -D include/aedis/detail/impl/*.ipp --target-directory $(incdir)/detail/impl

uninstall:
	rm -rf $(incdir)

.PHONY: clean
clean:
	rm -f $(remove)

$(tarball_name).tar.gz:
	git archive --format=tar.gz --prefix=$(tarball_dir)/ HEAD > $(tarball_name).tar.gz

.PHONY: dist
dist: $(tarball_name).tar.gz

