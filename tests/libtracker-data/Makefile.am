include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = $(TEST_PROGS)

TEST_PROGS += 								\
	tracker-sparql							\
	tracker-ontology

INCLUDES = 								\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-DTRACKER_COMPILATION						\
	-I$(top_srcdir)/src						\
	-I$(top_srcdir)/tests/common					\
	$(DBUS_CFLAGS)							\
	$(SQLITE3_CFLAGS)						\
	$(QDBM_CFLAGS)							\
	$(GMODULE_CFLAGS)						\
	$(GTHREAD_CFLAGS)						\
	$(GLIB2_CFLAGS)							\
	$(RAPTOR_CFLAGS)

tracker_sparql_SOURCES = 						\
	tracker-sparql-test.c

common_ldadd =								\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(top_builddir)/src/libtracker-db/libtracker-db.la	 	\
	$(top_builddir)/src/libtracker-data/libtracker-data.la	 	\
	$(GMODULE_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GLIB2_LIBS)							\
	$(RAPTOR_LIBS)

tracker_sparql_LDADD = $(common_ldadd)

tracker_ontology_SOURCES =						\
	tracker-ontology-test.c

tracker_ontology_LDADD = $(common_ldadd)

EXTRA_DIST = dawg-testcases
