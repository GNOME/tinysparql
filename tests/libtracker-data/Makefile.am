include $(top_srcdir)/Makefile.decl

SUBDIRS = 			\
	aggregates		\
	algebra			\
	anon			\
	bnode-coreference	\
	bound 			\
	datetime		\
	expr-ops		\
	functions		\
	graph			\
	nie			\
	nmo			\
	optional		\
	regex			\
	sort			\
	subqueries		\
	error			\
	backup			\
	turtle

noinst_PROGRAMS = $(TEST_PROGS)

TEST_PROGS += 								\
	tracker-sparql							\
	tracker-sparql-blank						\
	tracker-ontology						\
	tracker-backup

INCLUDES = 								\
	-DTRACKER_COMPILATION						\
	-DTOP_SRCDIR=\"$(top_srcdir)\"					\
	-DTOP_BUILDDIR=\"$(top_builddir)\"				\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-I$(top_srcdir)/src						\
	-I$(top_srcdir)/tests/common					\
	$(WARN_CFLAGS)							\
	$(GCOV_CFLAGS)							\
	$(DBUS_CFLAGS)							\
	$(SQLITE3_CFLAGS)						\
	$(GMODULE_CFLAGS)						\
	$(GTHREAD_CFLAGS)						\
	$(GLIB2_CFLAGS)

common_ldadd =								\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(top_builddir)/src/libtracker-db/libtracker-db.la	 	\
	$(top_builddir)/src/libtracker-data/libtracker-data.la	 	\
	$(top_builddir)/tests/common/libtracker-testcommon.la 		\
	$(GCOV_LIBS)							\
	$(GMODULE_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GLIB2_LIBS)

tracker_sparql_SOURCES = 						\
	tracker-sparql-test.c

tracker_sparql_LDADD = $(common_ldadd)

tracker_sparql_blank_SOURCES = 						\
	tracker-sparql-blank-test.c

tracker_sparql_blank_LDADD = $(common_ldadd)

tracker_ontology_SOURCES =						\
	tracker-ontology-test.c

tracker_ontology_LDADD = $(common_ldadd)

tracker_backup_SOURCES = 						\
	tracker-backup-test.c

tracker_backup_LDADD = $(common_ldadd)

EXTRA_DIST = dawg-testcases
