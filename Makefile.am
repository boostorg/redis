AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = -I m4

AM_COLOR_TESTS = always
DISTCHECK_CONFIGURE_FLAGS = CPPFLAGS="$(BOOST_CPPFLAGS) $(CPPFLAGS)" LDFLAGS="$(BOOST_LDFLAGS)"

AM_CPPFLAGS =
AM_CPPFLAGS += $(BOOST_CPPFLAGS)
#AM_CPPFLAGS += -I$(top_srcdir)/include

AM_LDFLAGS =
AM_LDFLAGS += -pthread

check_PROGRAMS =
check_PROGRAMS += intro_sync
check_PROGRAMS += intro
check_PROGRAMS += containers
check_PROGRAMS += serialization
check_PROGRAMS += adapter
check_PROGRAMS += test_low_level
if HAVE_CXX20
check_PROGRAMS += test_high_level
endif

EXTRA_PROGRAMS =
EXTRA_PROGRAMS += subscriber
EXTRA_PROGRAMS += commands
if HAVE_CXX20
EXTRA_PROGRAMS += echo_server
EXTRA_PROGRAMS += echo_server_direct
EXTRA_PROGRAMS += chat_room
EXTRA_PROGRAMS += echo_server_client
endif

CLEANFILES =
CLEANFILES += $(EXTRA_PROGRAMS)

.PHONY: all
all: $(check_PROGRAMS) $(EXTRA_PROGRAMS)

intro_sync_SOURCES = $(top_srcdir)/tests/intro_sync.cpp
commands_SOURCES = $(top_srcdir)/tools/commands.cpp
subscriber_SOURCES = $(top_srcdir)/examples/subscriber.cpp
test_low_level_SOURCES = $(top_srcdir)/tests/low_level.cpp
intro_SOURCES = $(top_srcdir)/examples/intro.cpp
containers_SOURCES = $(top_srcdir)/examples/containers.cpp
serialization_SOURCES = $(top_srcdir)/examples/serialization.cpp
adapter_SOURCES = $(top_srcdir)/examples/adapter.cpp
if HAVE_CXX20
test_high_level_SOURCES = $(top_srcdir)/tests/high_level.cpp
chat_room_SOURCES = $(top_srcdir)/examples/chat_room.cpp
echo_server_SOURCES = $(top_srcdir)/examples/echo_server.cpp
echo_server_direct_SOURCES = $(top_srcdir)/benchmarks/cpp/echo_server_direct.cpp
echo_server_client_SOURCES = $(top_srcdir)/benchmarks/cpp/echo_server_client.cpp
endif

nobase_include_HEADERS =\
  $(top_srcdir)/aedis/src.hpp\
  $(top_srcdir)/aedis/error.hpp\
  $(top_srcdir)/aedis/impl/error.ipp\
  $(top_srcdir)/aedis/detail/net.hpp\
  $(top_srcdir)/aedis/command.hpp\
  $(top_srcdir)/aedis/impl/command.ipp\
  $(top_srcdir)/aedis/connection.hpp\
  $(top_srcdir)/aedis/adapt.hpp\
  $(top_srcdir)/aedis/detail/connection_ops.hpp\
  $(top_srcdir)/aedis/aedis.hpp\
  $(top_srcdir)/aedis/adapter/detail/adapters.hpp\
  $(top_srcdir)/aedis/adapter/adapt.hpp\
  $(top_srcdir)/aedis/adapter/detail/response_traits.hpp\
  $(top_srcdir)/aedis/resp3/node.hpp\
  $(top_srcdir)/aedis/resp3/compose.hpp\
  $(top_srcdir)/aedis/resp3/detail/read_ops.hpp\
  $(top_srcdir)/aedis/resp3/detail/parser.hpp\
  $(top_srcdir)/aedis/resp3/type.hpp\
  $(top_srcdir)/aedis/resp3/read.hpp\
  $(top_srcdir)/aedis/resp3/exec.hpp\
  $(top_srcdir)/aedis/resp3/write.hpp\
  $(top_srcdir)/aedis/resp3/request.hpp\
  $(top_srcdir)/aedis/resp3/detail/impl/parser.ipp\
  $(top_srcdir)/aedis/resp3/impl/type.ipp

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
EXTRA_DIST += $(top_srcdir)/benchmarks/go/echo_server_direct.go
EXTRA_DIST += $(top_srcdir)/benchmarks/nodejs/echo_server_direct.js
EXTRA_DIST += $(top_srcdir)/benchmarks/nodejs/echo_server_over_redis.js
EXTRA_DIST += $(top_srcdir)/benchmarks/nodejs/package.json
EXTRA_DIST += $(top_srcdir)/benchmarks/nodejs/package-lock.json
EXTRA_DIST += $(top_srcdir)/benchmarks/rust/echo_server_direct/Cargo.lock
EXTRA_DIST += $(top_srcdir)/benchmarks/rust/echo_server_direct/Cargo.toml
EXTRA_DIST += $(top_srcdir)/benchmarks/rust/echo_server_direct/src/main.rs
EXTRA_DIST += $(top_srcdir)/benchmarks/rust/echo_server_over_redis/Cargo.lock
EXTRA_DIST += $(top_srcdir)/benchmarks/rust/echo_server_over_redis/Cargo.toml
EXTRA_DIST += $(top_srcdir)/benchmarks/rust/echo_server_over_redis/src/main.rs

.PHONY: doc
doc:
	rm -rf ../aedis-gh-pages/*
	doxygen doc/Doxyfile

