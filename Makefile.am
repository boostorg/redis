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
check_PROGRAMS += low_level_sync_intro
check_PROGRAMS += high_level_intro
check_PROGRAMS += high_level_aggregates
check_PROGRAMS += high_level_stl_containers
check_PROGRAMS += high_level_serialization
check_PROGRAMS += high_level_receiver
check_PROGRAMS += test_low_level
if HAVE_CXX20
check_PROGRAMS += low_level_async_intro
check_PROGRAMS += low_level_adapter
check_PROGRAMS += test_online
endif

EXTRA_PROGRAMS =
EXTRA_PROGRAMS += high_level_subscriber
EXTRA_PROGRAMS += high_level_sentinel
EXTRA_PROGRAMS += commands
if HAVE_CXX20
EXTRA_PROGRAMS += low_level_subscriber
EXTRA_PROGRAMS += high_level_echo_server
EXTRA_PROGRAMS += high_level_chat_room
endif

CLEANFILES =
CLEANFILES += $(EXTRA_PROGRAMS)

.PHONY: all
all: $(check_PROGRAMS) $(EXTRA_PROGRAMS)

high_level_intro_SOURCES = $(top_srcdir)/examples/high_level/intro.cpp
high_level_aggregates_SOURCES = $(top_srcdir)/examples/high_level/aggregates.cpp
high_level_stl_containers_SOURCES = $(top_srcdir)/examples/high_level/stl_containers.cpp
high_level_serialization_SOURCES = $(top_srcdir)/examples/high_level/serialization.cpp
high_level_receiver_SOURCES = $(top_srcdir)/examples/high_level/receiver.cpp
low_level_sync_intro_SOURCES = $(top_srcdir)/examples/low_level/sync_intro.cpp
commands_SOURCES = $(top_srcdir)/tools/commands.cpp
high_level_subscriber_SOURCES = $(top_srcdir)/examples/high_level/subscriber.cpp
high_level_sentinel_SOURCES = $(top_srcdir)/examples/high_level/sentinel.cpp
test_low_level_SOURCES = $(top_srcdir)/tests/low_level.cpp
if HAVE_CXX20
test_online_SOURCES = $(top_srcdir)/tests/online.cpp
low_level_async_intro_SOURCES = $(top_srcdir)/examples/low_level/async_intro.cpp
low_level_subscriber_SOURCES = $(top_srcdir)/examples/low_level/subscriber.cpp
low_level_adapter_SOURCES = $(top_srcdir)/examples/low_level/adapter.cpp
high_level_echo_server_SOURCES = $(top_srcdir)/examples/high_level/echo_server.cpp
high_level_chat_room_SOURCES = $(top_srcdir)/examples/high_level/chat_room.cpp
endif

nobase_include_HEADERS =\
  $(top_srcdir)/aedis/src.hpp\
  $(top_srcdir)/aedis/redis/command.hpp\
  $(top_srcdir)/aedis/generic/client.hpp\
  $(top_srcdir)/aedis/generic/receiver_base.hpp\
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
  $(top_srcdir)/examples/high_level/user_session.hpp\
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

