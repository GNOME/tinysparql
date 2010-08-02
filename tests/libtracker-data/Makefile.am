include $(top_srcdir)/Makefile.decl

SUBDIRS = 			\
	aggregates		\
	algebra			\
	anon			\
	ask			\
	basic			\
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

#
# NOTE: Some of these tests have been disabled (by Martyn) temporarily
# 	because make distcheck fails. The reason for this is that it
# 	depends files being installed to $prefix/share which have not
# 	been installed at this point.
#
#       These tests include:
#
#	  tracker-db-manager-unattach
#	  tracker-db-manager-attach
#	  tracker-db-manager-custom
#


TEST_PROGS += 								\
	tracker-sparql							\
	tracker-sparql-blank						\
	tracker-ontology						\
	tracker-backup							\
	tracker-ontology-change						\
	tracker-db-journal						\
	tracker-db-dbus

# 	tracker-db-manager-unattach					\
# 	tracker-db-manager-attach					\
# 	tracker-db-manager-custom

INCLUDES = 								\
	-DTRACKER_COMPILATION						\
	-DTOP_SRCDIR=\"$(abs_top_srcdir)\"				\
	-DTOP_BUILDDIR=\"$(abs_top_builddir)\"				\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-I$(top_srcdir)/src						\
	-I$(top_srcdir)/tests/common					\
	-I$(top_builddir)/src						\
	-I$(top_srcdir)/src/libtracker-client				\
	-I$(top_builddir)/src/libtracker-client				\
	-I$(top_srcdir)/src/libtracker-data				\
	-I$(top_builddir)/src/libtracker-data				\
	$(WARN_CFLAGS)							\
	$(GCOV_CFLAGS)							\
	$(DBUS_CFLAGS)							\
	$(SQLITE3_CFLAGS)						\
	$(GMODULE_CFLAGS)						\
	$(GTHREAD_CFLAGS)						\
	$(GLIB2_CFLAGS)

common_ldadd =								\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
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

tracker_ontology_change_SOURCES =					\
        tracker-ontology-change-test.c

tracker_ontology_change_LDADD = $(common_ldadd)

tracker_backup_SOURCES = 						\
	tracker-backup-test.c

tracker_backup_LDADD = $(common_ldadd)


tracker_db_journal_SOURCES = 						\
	tracker-db-journal.c

tracker_db_journal_LDADD =						\
	$(top_builddir)/src/libtracker-data/libtracker-data.la 		\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(top_builddir)/tests/common/libtracker-testcommon.la 		\
	$(SQLITE3_LIBS)							\
	$(GMODULE_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GLIB2_LIBS)							\
	-lz

tracker_db_dbus_SOURCES = 						\
	tracker-db-dbus-test.c

tracker_db_dbus_LDADD =						\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(top_builddir)/src/libtracker-data/libtracker-data.la 		\
	$(top_builddir)/tests/common/libtracker-testcommon.la 		\
	$(SQLITE3_LIBS)							\
	$(GMODULE_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GLIB2_LIBS)							\
	-lz

#
# tracker_db_manager_attach_SOURCES = 					\
# 	tracker-db-manager-test-attach.c 				\
# 	tracker-db-manager-common.c					\
# 	tracker-db-manager-common.h
#
# tracker_db_manager_attach_LDADD =					\
# 	$(top_builddir)/src/libtracker-data/libtracker-data.la 		\
# 	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
# 	$(top_builddir)/tests/common/libtracker-testcommon.la 		\
# 	$(SQLITE3_LIBS)							\
# 	$(GMODULE_LIBS)							\
# 	$(GTHREAD_LIBS)							\
# 	$(GLIB2_LIBS)							\
# 	-lz
#
# tracker_db_manager_unattach_SOURCES = 				\
# 	tracker-db-manager-test-unattach.c 				\
# 	tracker-db-manager-common.c					\
# 	tracker-db-manager-common.h
#
# tracker_db_manager_unattach_LDADD =					\
# 	$(top_builddir)/src/libtracker-data/libtracker-data.la 		\
# 	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
# 	$(top_builddir)/tests/common/libtracker-testcommon.la 		\
# 	$(SQLITE3_LIBS)							\
# 	$(GMODULE_LIBS)							\
# 	$(GTHREAD_LIBS)							\
# 	$(GLIB2_LIBS)							\
# 	-lz
#
# tracker_db_manager_custom_SOURCES = 					\
# 	tracker-db-manager-test-custom.c 				\
# 	tracker-db-manager-common.c					\
# 	tracker-db-manager-common.h
#
# tracker_db_manager_custom_LDADD =					\
# 	$(top_builddir)/src/libtracker-data/libtracker-data.la 		\
# 	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
# 	$(top_builddir)/tests/common/libtracker-testcommon.la 		\
# 	$(SQLITE3_LIBS)							\
# 	$(GMODULE_LIBS)							\
# 	$(GTHREAD_LIBS)							\
# 	$(GLIB2_LIBS)							\
# 	-lz



EXTRA_DIST = \
	dawg-testcases				\
	change/source/99-example.ontology.v1	\
	change/source/99-example.ontology.v2	\
	change/source/99-example.ontology.v3	\
	change/source/99-example.ontology.v4	\
	change/source/99-example.ontology.v5	\
	change/source/99-example.ontology.v6	\
	change/source/99-example.ontology.v7	\
	change/test-1.out			\
	change/test-1.rq			\
	change/test-2.out			\
	change/test-2.rq			\
	change/test-3.out			\
	change/test-3.rq			\
	change/test-4.rq			\
	change/test-4.out			\
	change/test-5.out			\
	change/test-5.rq			\
	change/change-test-1.rq			\
	change/change-test-1.out		\
	change/change-test-2.rq			\
	change/change-test-2.out		\
	change/change-test-3.rq			\
	change/change-test-3.out		\
	change/updates/99-example.queries.v2	\
	change/updates/99-example.queries.v3	\
	change/updates/99-example.queries.v4	\
	change/updates/99-example.queries.v5	\
	change/updates/99-example.queries.v6	\
	change/updates/99-example.queries.v7	\
	ontologies/20-dc.ontology		\
	ontologies/31-nao.ontology		\
	ontologies/90-tracker.ontology
