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
noinst_PROGRAMS += basic1
basic1_SOURCES = $(top_srcdir)/examples/basic1.cpp
basic1_CPPFLAGS = $(MY_CPPFLAGS)
basic1_LDADD = $(MY_LDADD)

noinst_PROGRAMS += basic2
basic2_SOURCES = $(top_srcdir)/examples/basic2.cpp
basic2_CPPFLAGS = $(MY_CPPFLAGS)
basic2_LDADD = $(MY_LDADD)

noinst_PROGRAMS += basic3
basic3_SOURCES = $(top_srcdir)/examples/basic3.cpp
basic3_CPPFLAGS = $(MY_CPPFLAGS)
basic3_LDADD = $(MY_LDADD)

noinst_PROGRAMS += basic4
basic4_SOURCES = $(top_srcdir)/examples/basic4.cpp
basic4_CPPFLAGS = $(MY_CPPFLAGS)
basic4_LDADD = $(MY_LDADD)

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

noinst_PROGRAMS += myclient1
myclient1_SOURCES = $(top_srcdir)/examples/myclient1.cpp
myclient1_CPPFLAGS = $(MY_CPPFLAGS)
myclient1_LDADD = $(MY_LDADD)

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

