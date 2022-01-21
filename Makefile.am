ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = subdir-objects

AM_COLOR_TESTS = always
DISTCHECK_CONFIGURE_FLAGS = CPPFLAGS="$(BOOST_CPPFLAGS) $(CPPFLAGS)" LDFLAGS="$(BOOST_LDFLAGS)"

MY_CPPFLAGS =
MY_CPPFLAGS += $(BOOST_CPPFLAGS)
MY_CPPFLAGS += -I$(top_srcdir)/

MY_LDADD =
MY_LDADD += -lpthread

noinst_PROGRAMS =
noinst_PROGRAMS += intro
intro_SOURCES = $(top_srcdir)/examples/intro.cpp
intro_CPPFLAGS = $(MY_CPPFLAGS)
intro_LDADD = $(MY_LDADD)

noinst_PROGRAMS += subscriber
subscriber_SOURCES = $(top_srcdir)/examples/subscriber.cpp
subscriber_CPPFLAGS = $(MY_CPPFLAGS)
subscriber_LDADD = $(MY_LDADD)

noinst_PROGRAMS += sets
sets_SOURCES = $(top_srcdir)/examples/sets.cpp
sets_CPPFLAGS = $(MY_CPPFLAGS)
sets_LDADD = $(MY_LDADD)

noinst_PROGRAMS += hashes
hashes_SOURCES = $(top_srcdir)/examples/hashes.cpp
hashes_CPPFLAGS = $(MY_CPPFLAGS)
hashes_LDADD = $(MY_LDADD)

noinst_PROGRAMS += serialization
serialization_SOURCES = $(top_srcdir)/examples/serialization.cpp
serialization_CPPFLAGS = $(MY_CPPFLAGS)
serialization_LDADD = $(MY_LDADD)

noinst_PROGRAMS += nested_response
nested_response_SOURCES = $(top_srcdir)/examples/nested_response.cpp
nested_response_CPPFLAGS = $(MY_CPPFLAGS)
nested_response_LDADD = $(MY_LDADD)

noinst_PROGRAMS += lists
lists_SOURCES = $(top_srcdir)/examples/lists.cpp
lists_CPPFLAGS = $(MY_CPPFLAGS)
lists_LDADD = $(MY_LDADD)

noinst_PROGRAMS += echo_server
echo_server_SOURCES = $(top_srcdir)/examples/echo_server.cpp
echo_server_CPPFLAGS = $(MY_CPPFLAGS)
echo_server_LDADD = $(MY_LDADD)

noinst_PROGRAMS += chat_room
chat_room_SOURCES = $(top_srcdir)/examples/chat_room.cpp
chat_room_CPPFLAGS = $(MY_CPPFLAGS)
chat_room_LDADD = $(MY_LDADD)

noinst_PROGRAMS += key_expiration
key_expiration_SOURCES = $(top_srcdir)/examples/key_expiration.cpp
key_expiration_CPPFLAGS = $(MY_CPPFLAGS)
key_expiration_LDADD = $(MY_LDADD)

noinst_PROGRAMS += response_adapter
response_adapter_SOURCES = $(top_srcdir)/examples/response_adapter.cpp
response_adapter_CPPFLAGS = $(MY_CPPFLAGS)
response_adapter_LDADD = $(MY_LDADD)

noinst_PROGRAMS += sync
sync_SOURCES = $(top_srcdir)/examples/sync.cpp
sync_CPPFLAGS = $(MY_CPPFLAGS)
sync_LDADD = $(MY_LDADD)

noinst_PROGRAMS += redis_client
redis_client_SOURCES = $(top_srcdir)/examples/redis_client.cpp
redis_client_CPPFLAGS = $(MY_CPPFLAGS)
redis_client_LDADD = $(MY_LDADD)

noinst_PROGRAMS += commands
commands_SOURCES = $(top_srcdir)/tools/commands.cpp
commands_CPPFLAGS = $(MY_CPPFLAGS)
commands_LDADD = $(MY_LDADD)

noinst_PROGRAMS += test_offline
test_offline_SOURCES = $(top_srcdir)/tests/offline.cpp
test_offline_CPPFLAGS = $(MY_CPPFLAGS)
test_offline_LDADD = $(MY_LDADD)

noinst_PROGRAMS += test_online
test_online_SOURCES = $(top_srcdir)/tests/online.cpp
test_online_CPPFLAGS = $(MY_CPPFLAGS)
test_online_LDADD = $(MY_LDADD)

nobase_include_HEADERS =\
  $(top_srcdir)/aedis/src.hpp\
  $(top_srcdir)/aedis/net.hpp\
  $(top_srcdir)/aedis/command.hpp\
  $(top_srcdir)/aedis/aedis.hpp\
  $(top_srcdir)/aedis/resp3/adapter/detail/adapters.hpp\
  $(top_srcdir)/aedis/resp3/adapter/error.hpp\
  $(top_srcdir)/aedis/resp3/adapt.hpp\
  $(top_srcdir)/aedis/resp3/detail/composer.hpp\
  $(top_srcdir)/aedis/resp3/detail/read_ops.hpp\
  $(top_srcdir)/aedis/resp3/detail/parser.hpp\
  $(top_srcdir)/aedis/resp3/client.hpp\
  $(top_srcdir)/aedis/resp3/serializer.hpp\
  $(top_srcdir)/aedis/resp3/response_traits.hpp\
  $(top_srcdir)/aedis/resp3/node.hpp\
  $(top_srcdir)/aedis/resp3/error.hpp\
  $(top_srcdir)/aedis/resp3/type.hpp\
  $(top_srcdir)/aedis/resp3/read.hpp\
  $(top_srcdir)/aedis/impl/command.ipp\
  $(top_srcdir)/aedis/resp3/detail/impl/parser.ipp\
  $(top_srcdir)/aedis/resp3/impl/type.ipp\
  $(top_srcdir)/aedis/resp3/impl/client.ipp\
  $(top_srcdir)/aedis/resp3/impl/node.ipp

nobase_noinst_HEADERS =\
  $(top_srcdir)/examples/utils.ipp\
  $(top_srcdir)/examples/lib/responses.ipp\
  $(top_srcdir)/examples/lib/responses.hpp\
  $(top_srcdir)/examples/lib/user_session.hpp\
  $(top_srcdir)/examples/lib/user_session.ipp\
  $(top_srcdir)/examples/src.hpp\
  $(top_srcdir)/tests/check.hpp\
  $(top_srcdir)/tests/test_stream.hpp

TESTS = test_offline test_offline

EXTRA_DIST =
EXTRA_DIST += $(top_srcdir)/README.md
EXTRA_DIST += $(top_srcdir)/doc/Doxyfile
EXTRA_DIST += $(top_srcdir)/doc/DoxygenLayout.xml
EXTRA_DIST += $(top_srcdir)/doc/aedis.css
EXTRA_DIST += $(top_srcdir)/doc/htmlfooter.html
EXTRA_DIST += $(top_srcdir)/doc/htmlheader.html

CLEANFILES =
#CLEANFILES += Makefile.dep

.PHONY: doc
doc:
	rm -rf /tmp/aedis
	doxygen doc/Doxyfile

.PHONY: deb
deb: dist
	export CPPFLAGS="$(BOOST_CPPFLAGS) $(CPPFLAGS)" &&\
	export LDFLAGS="$(BOOST_LDFLAGS)" &&\
	rm -rf tmp &&\
	mkdir tmp &&\
	mv $(distdir).tar.gz $(distdir)-1.tar.gz &&\
	mv $(distdir)-1.tar.gz tmp &&\
	cd tmp &&\
	ln $(distdir)-1.tar.gz $(PACKAGE)_$(VERSION).orig.tar.gz &&\
	tar -xvvzf $(distdir)-1.tar.gz &&\
	cd $(distdir)/debian; debuild --no-sign -j1

