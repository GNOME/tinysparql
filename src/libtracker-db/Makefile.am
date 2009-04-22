include $(top_srcdir)/Makefile.decl

INCLUDES =								\
	-DSHAREDIR=\""$(datadir)"\"					\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-DPKGLIBDIR=\""$(libdir)/tracker-$(TRACKER_API_VERSION)"\"	\
	-DTRACKER_COMPILATION						\
	-I$(top_srcdir)/src						\
	$(WARN_CFLAGS)							\
	$(GLIB2_CFLAGS)							\
	$(GCOV_CFLAGS)							\
	$(DBUS_CFLAGS)							\
	$(SQLITE3_CFLAGS)

libtracker_dbdir = $(libdir)/tracker-$(TRACKER_API_VERSION)
libtracker_db_LTLIBRARIES = libtracker-db.la

libtracker_db_la_SOURCES = 						\
	tracker-db-dbus.c						\
	tracker-db-interface.c						\
	tracker-db-interface-sqlite.c					\
	tracker-db-manager.c

noinst_HEADERS =							\
	tracker-db-dbus.h						\
	tracker-db-interface.h						\
	tracker-db-interface-sqlite.h					\
	tracker-db-manager.h 

libtracker_db_la_LDFLAGS =						\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)

libtracker_db_la_LIBADD = 						\
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(SQLITE3_LIBS)							\
	$(DBUS_LIBS)							\
	$(GCOV_LIBS)							\
	$(GLIB2_LIBS)							\
	-lz

EXTRA_DIST = libtracker-db.vapi

