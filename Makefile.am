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
check_PROGRAMS += serialization_sync
check_PROGRAMS += intro_high_level
check_PROGRAMS += aggregates_high_level
check_PROGRAMS += test_low_level
if HAVE_CXX20
check_PROGRAMS += transaction
check_PROGRAMS += custom_adapter
check_PROGRAMS += test_online
endif

EXTRA_PROGRAMS =
EXTRA_PROGRAMS += subscriber_high_level
EXTRA_PROGRAMS += commands
if HAVE_CXX20
EXTRA_PROGRAMS += subscriber
EXTRA_PROGRAMS += echo_server
EXTRA_PROGRAMS += chat_room
endif

CLEANFILES =
CLEANFILES += $(EXTRA_PROGRAMS)

.PHONY: all
all: $(check_PROGRAMS) $(EXTRA_PROGRAMS)

intro_high_level_SOURCES = $(top_srcdir)/examples/intro_high_level.cpp
aggregates_high_level_SOURCES = $(top_srcdir)/examples/aggregates_high_level.cpp
intro_sync_SOURCES = $(top_srcdir)/examples/intro_sync.cpp
serialization_sync_SOURCES = $(top_srcdir)/examples/serialization_sync.cpp
commands_SOURCES = $(top_srcdir)/tools/commands.cpp
subscriber_high_level_SOURCES = $(top_srcdir)/examples/subscriber_high_level.cpp
test_low_level_SOURCES = $(top_srcdir)/tests/low_level.cpp
if HAVE_CXX20
test_online_SOURCES = $(top_srcdir)/tests/online.cpp
transaction_SOURCES = $(top_srcdir)/examples/transaction.cpp
subscriber_SOURCES = $(top_srcdir)/examples/subscriber.cpp
custom_adapter_SOURCES = $(top_srcdir)/examples/custom_adapter.cpp
echo_server_SOURCES = $(top_srcdir)/examples/echo_server.cpp
chat_room_SOURCES = $(top_srcdir)/examples/chat_room.cpp
endif

nobase_include_HEADERS =\
  $(top_srcdir)/aedis/src.hpp\
  $(top_srcdir)/aedis/redis/command.hpp\
  $(top_srcdir)/aedis/generic/client.hpp\
  $(top_srcdir)/aedis/generic/serializer.hpp\
  $(top_srcdir)/aedis/generic/detail/client_ops.hpp\
  $(top_srcdir)/aedis/sentinel/command.hpp\
  $(top_srcdir)/aedis/aedis.hpp\
  $(top_srcdir)/aedis/adapter/detail/adapters.hpp\
  $(top_srcdir)/aedis/adapter/error.hpp\
  $(top_srcdir)/aedis/adapter/impl/error.ipp\
  $(top_srcdir)/aedis/adapter/adapt.hpp\
  $(top_srcdir)/aedis/adapter/response_traits.hpp\
  $(top_srcdir)/aedis/resp3/node.hpp\
  $(top_srcdir)/aedis/resp3/compose.hpp\
  $(top_srcdir)/aedis/resp3/detail/read_ops.hpp\
  $(top_srcdir)/aedis/resp3/detail/parser.hpp\
  $(top_srcdir)/aedis/resp3/error.hpp\
  $(top_srcdir)/aedis/resp3/impl/error.ipp\
  $(top_srcdir)/aedis/resp3/type.hpp\
  $(top_srcdir)/aedis/resp3/read.hpp\
  $(top_srcdir)/aedis/redis/impl/command.ipp\
  $(top_srcdir)/aedis/sentinel/impl/command.ipp\
  $(top_srcdir)/aedis/resp3/detail/impl/parser.ipp\
  $(top_srcdir)/aedis/resp3/impl/type.ipp

nobase_noinst_HEADERS =\
  $(top_srcdir)/examples/user_session.hpp\
  $(top_srcdir)/examples/print.hpp\
  $(top_srcdir)/examples/mystruct.hpp\
  $(top_srcdir)/tests/check.hpp

TESTS = $(check_PROGRAMS)

EXTRA_DIST =
EXTRA_DIST += $(top_srcdir)/README.md
EXTRA_DIST += $(top_srcdir)/doc/DoxygenLayout.xml
EXTRA_DIST += $(top_srcdir)/doc/aedis.css
EXTRA_DIST += $(top_srcdir)/doc/htmlfooter.html
EXTRA_DIST += $(top_srcdir)/doc/htmlheader.html

.PHONY: doc
doc:
	rm -rf ../aedis-gh-pages/*
	doxygen doc/Doxyfile

