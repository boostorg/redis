ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = subdir-objects

AM_COLOR_TESTS = always

Makefile.dep:
	-$(CXX) -MM ./src/*.cpp > $@

-include Makefile.dep

MY_CPPFLAGS =
MY_CPPFLAGS += $(BOOST_CPPFLAGS)
MY_CPPFLAGS += -I$(top_srcdir)/include

MY_LDADD =
MY_LDADD += libaedis.a
MY_LDADD += -lpthread

noinst_LIBRARIES = libaedis.a
libaedis_a_SOURCES =
libaedis_a_SOURCES += $(top_srcdir)/include/aedis/aedis.hpp
libaedis_a_SOURCES += $(top_srcdir)/include/aedis/buffers.hpp
libaedis_a_SOURCES += $(top_srcdir)/include/aedis/net.hpp
libaedis_a_SOURCES += $(top_srcdir)/include/aedis/connection.hpp
libaedis_a_SOURCES += $(top_srcdir)/include/aedis/pipeline.hpp
libaedis_a_SOURCES += $(top_srcdir)/include/aedis/type.hpp
libaedis_a_SOURCES += $(top_srcdir)/include/aedis/command.hpp
libaedis_a_SOURCES += $(top_srcdir)/include/aedis/version.hpp
libaedis_a_SOURCES += $(top_srcdir)/include/aedis/impl/connection.ipp
libaedis_a_SOURCES += $(top_srcdir)/include/aedis/impl/src.hpp
libaedis_a_SOURCES += $(top_srcdir)/include/aedis/impl/type.ipp
libaedis_a_SOURCES += $(top_srcdir)/include/aedis/impl/command.ipp
libaedis_a_SOURCES += $(top_srcdir)/include/aedis/detail/write.hpp
libaedis_a_SOURCES += $(top_srcdir)/include/aedis/detail/read.hpp
libaedis_a_SOURCES += $(top_srcdir)/include/aedis/detail/parser.hpp
libaedis_a_SOURCES += $(top_srcdir)/include/aedis/detail/response_adapter_base.hpp
libaedis_a_SOURCES += $(top_srcdir)/include/aedis/detail/responses.hpp
libaedis_a_SOURCES += $(top_srcdir)/include/aedis/detail/utils.hpp
libaedis_a_SOURCES += $(top_srcdir)/include/aedis/detail/impl/read.ipp
libaedis_a_SOURCES += $(top_srcdir)/include/aedis/detail/impl/parser.ipp
libaedis_a_SOURCES += $(top_srcdir)/src/aedis.cpp
libaedis_a_CPPFLAGS = $(MY_CPPFLAGS)

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

noinst_PROGRAMS += response_queue
response_queue_SOURCES = $(top_srcdir)/examples/response_queue.cpp
response_queue_CPPFLAGS = $(MY_CPPFLAGS)
response_queue_LDADD = $(MY_LDADD)

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

TESTS = test_offline test_offline

EXTRA_DIST =
EXTRA_DIST += $(top_srcdir)/debian
EXTRA_DIST += $(top_srcdir)/README.md

CLEANFILES =
CLEANFILES += Makefile.dep

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

