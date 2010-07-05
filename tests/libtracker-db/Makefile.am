include $(top_srcdir)/Makefile.decl

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

INCLUDES = 								\
	-DTOP_SRCDIR=\"$(top_srcdir)\"					\
	-DTOP_BUILDDIR=\"$(top_builddir)\"				\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-DTRACKER_COMPILATION						\
	-I$(top_srcdir)/src						\
	-I$(top_srcdir)/tests/common					\
	$(WARN_CFLAGS)							\
	$(GLIB2_CFLAGS)							\
	$(GCOV_CFLAGS)							\
	$(GMODULE_CFLAGS)						\
	$(GTHREAD_CFLAGS)						\
	$(DBUS_CFLAGS)							\
	$(SQLITE3_CFLAGS)

TEST_PROGS +=								\
	tracker-db-journal						\
	tracker-db-dbus

# 	tracker-db-manager-unattach					\
# 	tracker-db-manager-attach					\
# 	tracker-db-manager-custom


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

