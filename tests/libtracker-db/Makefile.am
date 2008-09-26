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
#	  union-performance
#         tracker-db-dbus
#	  tracker-db-manager-unattach
#	  tracker-db-manager-attach
#	  tracker-db-manager-custom
#

TEST_PROGS += 								\
	tracker-index-writer

INCLUDES = 								\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-I$(top_srcdir)/src						\
	-I$(top_srcdir)/tests/common					\
	$(DBUS_CFLAGS)							\
	$(SQLITE3_CFLAGS)						\
	$(QDBM_CFLAGS)							\
	$(GMODULE_CFLAGS)						\
	$(GTHREAD_CFLAGS)						\
	$(GLIB2_CFLAGS)

# TEST_PROGS +=								\
#	union-performance						\
#	tracker-index-reader						\
# 	tracker-db-dbus							\
# 	tracker-db-manager-unattach					\
# 	tracker-db-manager-attach					\
# 	tracker-db-manager-custom					\
#
# union_performance_SOURCES =						\
# 	tracker-db-manager-common.h					\
# 	tracker-db-manager-common.c					\
# 	union-performance.c
#
# union_performance_LDADD =                                            	\
# 	$(top_builddir)/src/libtracker-common/libtracker-common.la      \
# 	$(top_builddir)/src/libtracker-db/libtracker-db.la              \
# 	$(top_builddir)/tests/common/libtracker-testcommon.la           \
# 	$(SQLITE3_LIBS)                                                 \
# 	$(QDBM_LIBS)                                                    \
# 	$(GMODULE_LIBS)                                                 \
# 	$(GTHREAD_LIBS)                                                 \
# 	$(GLIB2_LIBS)                                                   \
# 	-lz
#
# tracker_db_manager_attach_SOURCES = 					\
# 	tracker-db-manager-test-attach.c 				\
# 	tracker-db-manager-common.c					\
# 	tracker-db-manager-common.h
#
# tracker_db_manager_attach_LDADD =					\
# 	$(top_builddir)/src/libtracker-db/libtracker-db.la 		\
# 	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
# 	$(top_builddir)/tests/common/libtracker-testcommon.la 		\
# 	$(SQLITE3_LIBS)							\
# 	$(QDBM_LIBS)							\
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
# 	$(top_builddir)/src/libtracker-db/libtracker-db.la 		\
# 	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
# 	$(top_builddir)/tests/common/libtracker-testcommon.la 		\
# 	$(SQLITE3_LIBS)							\
# 	$(QDBM_LIBS)							\
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
# 	$(top_builddir)/src/libtracker-db/libtracker-db.la 		\
# 	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
# 	$(top_builddir)/tests/common/libtracker-testcommon.la 		\
# 	$(SQLITE3_LIBS)							\
# 	$(QDBM_LIBS)							\
# 	$(GMODULE_LIBS)							\
# 	$(GTHREAD_LIBS)							\
# 	$(GLIB2_LIBS)							\
# 	-lz
#
# tracker_db_dbus_SOURCES = 						\
# 	tracker-db-dbus-test.c 		
#
# tracker_db_dbus_LDADD =						\
# 	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
# 	$(top_builddir)/src/libtracker-db/libtracker-db.la 		\
# 	$(top_builddir)/tests/common/libtracker-testcommon.la 		\
# 	$(SQLITE3_LIBS)							\
# 	$(GMODULE_LIBS)							\
# 	$(GTHREAD_LIBS)							\
# 	$(GLIB2_LIBS)							\
# 	-lz
#
#
# tracker_index_reader_SOURCES = 					\
# 	tracker-index-reader-test.c 
#
# tracker_index_reader_LDADD = 						\
# 	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
# 	$(top_builddir)/src/libtracker-db/libtracker-db.la 		\
# 	$(top_builddir)/tests/common/libtracker-testcommon.la 		\
# 	$(GMODULE_LIBS) 						\
# 	$(GTHREAD_LIBS) 						\
# 	$(GLIB2_LIBS) 							\
# 	$(DBUS_LIBS) 							\
# 	$(GIO_LIBS) 							\
# 	$(QDBM_LIBS) 

tracker_index_writer_SOURCES = 						\
	tracker-index-writer-test.c

tracker_index_writer_LDADD =						\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(top_builddir)/src/libtracker-db/libtracker-db.la	 	\
	$(top_builddir)/tests/common/libtracker-testcommon.la 		\
	$(QDBM_LIBS)							\
	$(GMODULE_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GLIB2_LIBS)							

