AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = -I m4

AM_COLOR_TESTS = always
DISTCHECK_CONFIGURE_FLAGS = CPPFLAGS="$(BOOST_CPPFLAGS) $(CPPFLAGS)" LDFLAGS="$(BOOST_LDFLAGS)"

AM_CPPFLAGS =
AM_CPPFLAGS += $(BOOST_CPPFLAGS)

AM_LDFLAGS =
AM_LDFLAGS += -pthread

check_PROGRAMS =
check_PROGRAMS += intro
check_PROGRAMS += sets
check_PROGRAMS += hashes
check_PROGRAMS += serialization
check_PROGRAMS += multipurpose_response
check_PROGRAMS += lists
check_PROGRAMS += key_expiration
check_PROGRAMS += response_adapter
check_PROGRAMS += sync
check_PROGRAMS += multipurpose_client
check_PROGRAMS += test_offline
check_PROGRAMS += test_online
check_PROGRAMS += transaction

EXTRA_PROGRAMS =
EXTRA_PROGRAMS += subscriber
EXTRA_PROGRAMS += echo_server
EXTRA_PROGRAMS += chat_room
EXTRA_PROGRAMS += commands

CLEANFILES =
CLEANFILES += $(EXTRA_PROGRAMS)

.PHONY: all
all: $(check_PROGRAMS) $(EXTRA_PROGRAMS)

intro_SOURCES = $(top_srcdir)/examples/intro.cpp
sets_SOURCES = $(top_srcdir)/examples/sets.cpp
hashes_SOURCES = $(top_srcdir)/examples/hashes.cpp
serialization_SOURCES = $(top_srcdir)/examples/serialization.cpp
multipurpose_response_SOURCES = $(top_srcdir)/examples/multipurpose_response.cpp
lists_SOURCES = $(top_srcdir)/examples/lists.cpp
key_expiration_SOURCES = $(top_srcdir)/examples/key_expiration.cpp
response_adapter_SOURCES = $(top_srcdir)/examples/response_adapter.cpp
sync_SOURCES = $(top_srcdir)/examples/sync.cpp
multipurpose_client_SOURCES = $(top_srcdir)/examples/multipurpose_client.cpp
commands_SOURCES = $(top_srcdir)/tools/commands.cpp
test_offline_SOURCES = $(top_srcdir)/tests/offline.cpp
test_online_SOURCES = $(top_srcdir)/tests/online.cpp
subscriber_SOURCES = $(top_srcdir)/examples/subscriber.cpp
echo_server_SOURCES = $(top_srcdir)/examples/echo_server.cpp
chat_room_SOURCES = $(top_srcdir)/examples/chat_room.cpp
transaction_SOURCES = $(top_srcdir)/examples/transaction.cpp

nobase_include_HEADERS =\
  $(top_srcdir)/aedis/config.hpp\
  $(top_srcdir)/aedis/src.hpp\
  $(top_srcdir)/aedis/redis/command.hpp\
  $(top_srcdir)/aedis/sentinel/command.hpp\
  $(top_srcdir)/aedis/aedis.hpp\
  $(top_srcdir)/aedis/resp3/adapter/detail/adapters.hpp\
  $(top_srcdir)/aedis/resp3/adapter/error.hpp\
  $(top_srcdir)/aedis/resp3/adapt.hpp\
  $(top_srcdir)/aedis/resp3/detail/composer.hpp\
  $(top_srcdir)/aedis/resp3/detail/read_ops.hpp\
  $(top_srcdir)/aedis/resp3/detail/parser.hpp\
  $(top_srcdir)/aedis/resp3/serializer.hpp\
  $(top_srcdir)/aedis/resp3/response_traits.hpp\
  $(top_srcdir)/aedis/resp3/node.hpp\
  $(top_srcdir)/aedis/resp3/error.hpp\
  $(top_srcdir)/aedis/resp3/type.hpp\
  $(top_srcdir)/aedis/resp3/read.hpp\
  $(top_srcdir)/aedis/redis/impl/command.ipp\
  $(top_srcdir)/aedis/sentinel/impl/command.ipp\
  $(top_srcdir)/aedis/resp3/detail/impl/parser.ipp\
  $(top_srcdir)/aedis/resp3/impl/type.ipp\
  $(top_srcdir)/aedis/resp3/impl/node.ipp\
  $(top_srcdir)/aedis/redis/experimental/client.hpp\
  $(top_srcdir)/aedis/redis/experimental/impl/client.ipp

nobase_noinst_HEADERS =\
  $(top_srcdir)/examples/lib/net_utils.hpp\
  $(top_srcdir)/examples/lib/user_session.hpp\
  $(top_srcdir)/tests/check.hpp\
  $(top_srcdir)/tests/test_stream.hpp

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

