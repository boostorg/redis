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
libaedis_a_SOURCES += $(top_srcdir)/include/aedis/receiver_base.hpp
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
libaedis_a_SOURCES += $(top_srcdir)/include/aedis/detail/response_buffers.hpp
libaedis_a_SOURCES += $(top_srcdir)/include/aedis/detail/utils.hpp
libaedis_a_SOURCES += $(top_srcdir)/include/aedis/detail/impl/read.ipp
libaedis_a_SOURCES += $(top_srcdir)/include/aedis/detail/impl/parser.ipp
libaedis_a_SOURCES += $(top_srcdir)/src/aedis.cpp
libaedis_a_CPPFLAGS = $(MY_CPPFLAGS)

noinst_PROGRAMS =
noinst_PROGRAMS += async_basic
async_basic_SOURCES = $(top_srcdir)/examples/async_basic.cpp
async_basic_CPPFLAGS = $(MY_CPPFLAGS)
async_basic_LDADD = $(MY_LDADD)

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

