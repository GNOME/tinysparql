include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS += $(test_programs)

test_programs = \
	tracker-test

AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	-I$(top_srcdir)/tests/common                   \
	-DTEST_DOMAIN_ONTOLOGY_RULE=\""$(abs_top_srcdir)/src/tracker-store/default.rule"\" \
	-DTEST_ONTOLOGIES_DIR=\""$(abs_top_srcdir)/src/ontologies/nepomuk"\" \
	$(LIBTRACKER_SPARQL_CFLAGS)

LDADD =                                                \
	$(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS)                                  \
	$(LIBTRACKER_SPARQL_LIBS)

tracker_test_SOURCES =                                 \
	tracker-test.c

EXTRA_DIST += meson.build
