ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = subdir-objects

AM_COLOR_TESTS = always

Makefile.dep:
	-$(CXX) -MM ./src/*.cpp > $@

-include Makefile.dep

noinst_PROGRAMS =

noinst_LIBRARIES = libaedis.a
libaedis_a_SOURCES =
libaedis_a_SOURCES += $(top_srcdir)/aedis/aedis.hpp
libaedis_a_SOURCES += $(top_srcdir)/aedis/config.hpp
libaedis_a_SOURCES += $(top_srcdir)/aedis/connection.hpp
libaedis_a_SOURCES += $(top_srcdir)/aedis/read.hpp
libaedis_a_SOURCES += $(top_srcdir)/aedis/receiver_base.hpp
libaedis_a_SOURCES += $(top_srcdir)/aedis/request.hpp
libaedis_a_SOURCES += $(top_srcdir)/aedis/resp_types.hpp
libaedis_a_SOURCES += $(top_srcdir)/aedis/type.hpp
libaedis_a_SOURCES += $(top_srcdir)/aedis/command.hpp
libaedis_a_SOURCES += $(top_srcdir)/aedis/version.hpp
libaedis_a_SOURCES += $(top_srcdir)/aedis/write.hpp
libaedis_a_SOURCES += $(top_srcdir)/aedis/impl/connection.ipp
libaedis_a_SOURCES += $(top_srcdir)/aedis/impl/read.ipp
libaedis_a_SOURCES += $(top_srcdir)/aedis/impl/src.hpp
libaedis_a_SOURCES += $(top_srcdir)/aedis/impl/type.ipp
libaedis_a_SOURCES += $(top_srcdir)/aedis/impl/command.ipp
libaedis_a_SOURCES += $(top_srcdir)/aedis/detail/parser.hpp
libaedis_a_SOURCES += $(top_srcdir)/aedis/detail/response_base.hpp
libaedis_a_SOURCES += $(top_srcdir)/aedis/detail/response_buffers.hpp
libaedis_a_SOURCES += $(top_srcdir)/aedis/detail/response_types.hpp
libaedis_a_SOURCES += $(top_srcdir)/aedis/detail/responses.hpp
libaedis_a_SOURCES += $(top_srcdir)/aedis/detail/utils.hpp
libaedis_a_SOURCES += $(top_srcdir)/aedis/detail/impl/parser.ipp
libaedis_a_SOURCES += $(top_srcdir)/aedis/detail/impl/response_buffers.ipp
libaedis_a_SOURCES += $(top_srcdir)/src/aedis.cpp
libaedis_a_CPPFLAGS =
libaedis_a_CPPFLAGS += $(BOOST_CPPFLAGS)
libaedis_a_CPPFLAGS += -I$(top_srcdir)/include

noinst_PROGRAMS += async_basic
async_basic_SOURCES =
async_basic_SOURCES += $(top_srcdir)/examples/async_basic.cpp
async_basic_CPPFLAGS =
async_basic_CPPFLAGS += $(BOOST_CPPFLAGS)
async_basic_CPPFLAGS += -I$(top_srcdir)/include
async_basic_LDADD =
async_basic_LDADD += libaedis.a
async_basic_LDADD += -lpthread

noinst_PROGRAMS += test
test_SOURCES =
test_SOURCES += $(top_srcdir)/tests/general.cpp
test_CPPFLAGS =
test_CPPFLAGS += $(BOOST_CPPFLAGS)
test_CPPFLAGS += -I$(top_srcdir)/include
test_LDADD =
test_LDADD += libaedis.a
test_LDADD += -lpthread

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

