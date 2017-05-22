include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS += $(test_programs)

test_programs = gvdb-test

AM_CPPFLAGS = \
	$(BUILD_CFLAGS) \
	-I$(top_srcdir)/src \
	-I$(top_builddir)/src \
	$(GVDB_CFLAGS)

gvdb_test_SOURCES = gvdb-test.c
gvdb_test_LDADD = \
	$(top_builddir)/src/gvdb/libgvdb.la \
	$(BUILD_LIBS) \
	$(GVDB_LIBS)

EXTRA_DIST += meson.build
