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
noinst_PROGRAMS += intro1
intro1_SOURCES = $(top_srcdir)/examples/intro1.cpp
intro1_CPPFLAGS = $(MY_CPPFLAGS)
intro1_LDADD = $(MY_LDADD)

noinst_PROGRAMS += intro2
intro2_SOURCES = $(top_srcdir)/examples/intro2.cpp
intro2_CPPFLAGS = $(MY_CPPFLAGS)
intro2_LDADD = $(MY_LDADD)

noinst_PROGRAMS += intro3
intro3_SOURCES = $(top_srcdir)/examples/intro3.cpp
intro3_CPPFLAGS = $(MY_CPPFLAGS)
intro3_LDADD = $(MY_LDADD)

noinst_PROGRAMS += intro4
intro4_SOURCES = $(top_srcdir)/examples/intro4.cpp
intro4_CPPFLAGS = $(MY_CPPFLAGS)
intro4_LDADD = $(MY_LDADD)

noinst_PROGRAMS += containers
containers_SOURCES = $(top_srcdir)/examples/containers.cpp
containers_CPPFLAGS = $(MY_CPPFLAGS)
containers_LDADD = $(MY_LDADD)

noinst_PROGRAMS += custom_response1
custom_response1_SOURCES = $(top_srcdir)/examples/custom_response1.cpp
custom_response1_CPPFLAGS = $(MY_CPPFLAGS)
custom_response1_LDADD = $(MY_LDADD)

noinst_PROGRAMS += custom_response2
custom_response2_SOURCES = $(top_srcdir)/examples/custom_response2.cpp
custom_response2_CPPFLAGS = $(MY_CPPFLAGS)
custom_response2_LDADD = $(MY_LDADD)

noinst_PROGRAMS += echo_server
echo_server_SOURCES = $(top_srcdir)/examples/echo_server.cpp
echo_server_CPPFLAGS = $(MY_CPPFLAGS)
echo_server_LDADD = $(MY_LDADD)

noinst_PROGRAMS += test
test_SOURCES = $(top_srcdir)/tests/general.cpp
test_CPPFLAGS = $(MY_CPPFLAGS)
test_LDADD = $(MY_LDADD)

TESTS = test

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

