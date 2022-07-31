AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = -I m4

AM_COLOR_TESTS = always
DISTCHECK_CONFIGURE_FLAGS = CPPFLAGS="$(BOOST_CPPFLAGS) $(CPPFLAGS)" LDFLAGS="$(BOOST_LDFLAGS)"

AM_CPPFLAGS =
AM_CPPFLAGS += $(BOOST_CPPFLAGS)
AM_CPPFLAGS += -I$(top_srcdir)/include

AM_LDFLAGS =
AM_LDFLAGS += -pthread

SUBDIRS = include

check_PROGRAMS =
check_PROGRAMS += low_level_sync
check_PROGRAMS += intro
check_PROGRAMS += intro_sync
check_PROGRAMS += containers
check_PROGRAMS += serialization
check_PROGRAMS += test_low_level
check_PROGRAMS += test_high_level

EXTRA_PROGRAMS =
if HAVE_COROUTINES
EXTRA_PROGRAMS += subscriber
EXTRA_PROGRAMS += subscriber_sync
EXTRA_PROGRAMS += echo_server
EXTRA_PROGRAMS += echo_server_direct
EXTRA_PROGRAMS += chat_room
EXTRA_PROGRAMS += echo_server_client
endif

CLEANFILES =
CLEANFILES += $(EXTRA_PROGRAMS)

.PHONY: all
all: $(check_PROGRAMS) $(EXTRA_PROGRAMS)

low_level_sync_SOURCES = $(top_srcdir)/tests/low_level_sync.cpp
test_low_level_SOURCES = $(top_srcdir)/tests/low_level.cpp
intro_SOURCES = $(top_srcdir)/examples/intro.cpp
intro_sync_SOURCES = $(top_srcdir)/examples/intro_sync.cpp
containers_SOURCES = $(top_srcdir)/examples/containers.cpp
serialization_SOURCES = $(top_srcdir)/examples/serialization.cpp
test_high_level_SOURCES = $(top_srcdir)/tests/high_level.cpp
subscriber_sync_SOURCES = $(top_srcdir)/examples/subscriber_sync.cpp
if HAVE_COROUTINES
subscriber_SOURCES = $(top_srcdir)/examples/subscriber.cpp
chat_room_SOURCES = $(top_srcdir)/examples/chat_room.cpp
echo_server_SOURCES = $(top_srcdir)/examples/echo_server.cpp
echo_server_direct_SOURCES = $(top_srcdir)/benchmarks/cpp/asio/echo_server_direct.cpp
echo_server_client_SOURCES = $(top_srcdir)/benchmarks/cpp/asio/echo_server_client.cpp
endif

nobase_noinst_HEADERS =\
  $(top_srcdir)/examples/print.hpp\
  $(top_srcdir)/tests/check.hpp

TESTS = $(check_PROGRAMS)

EXTRA_DIST =
EXTRA_DIST += $(top_srcdir)/README.md
EXTRA_DIST += $(top_srcdir)/doc/DoxygenLayout.xml
EXTRA_DIST += $(top_srcdir)/doc/aedis.css
EXTRA_DIST += $(top_srcdir)/doc/htmlfooter.html
EXTRA_DIST += $(top_srcdir)/doc/htmlheader.html
EXTRA_DIST += $(top_srcdir)/benchmarks/benchmarks.md
EXTRA_DIST += $(top_srcdir)/benchmarks/benchmarks.tex
EXTRA_DIST += $(top_srcdir)/benchmarks/c/libuv/echo_server_direct.c
EXTRA_DIST += $(top_srcdir)/benchmarks/c/libuv/README.md
EXTRA_DIST += $(top_srcdir)/benchmarks/go/echo_server_direct.go
EXTRA_DIST += $(top_srcdir)/benchmarks/nodejs/echo_server_direct/echo_server_direct.js
EXTRA_DIST += $(top_srcdir)/benchmarks/nodejs/echo_server_direct/package.json
EXTRA_DIST += $(top_srcdir)/benchmarks/nodejs/echo_server_over_redis/echo_server_over_redis.js
EXTRA_DIST += $(top_srcdir)/benchmarks/nodejs/echo_server_over_redis/package.json
EXTRA_DIST += $(top_srcdir)/benchmarks/rust/echo_server_direct/Cargo.toml
EXTRA_DIST += $(top_srcdir)/benchmarks/rust/echo_server_direct/src/main.rs
EXTRA_DIST += $(top_srcdir)/benchmarks/rust/echo_server_over_redis/Cargo.toml
EXTRA_DIST += $(top_srcdir)/benchmarks/rust/echo_server_over_redis/src/main.rs

.PHONY: doc
doc:
	doxygen doc/Doxyfile

.PHONY: bench
bench:
	pdflatex --jobname=echo-f0 benchmarks/benchmarks.tex
	pdflatex --jobname=echo-f1 benchmarks/benchmarks.tex
