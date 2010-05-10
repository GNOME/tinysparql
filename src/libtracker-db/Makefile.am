include $(top_srcdir)/Makefile.decl

INCLUDES =								\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-DTRACKER_COMPILATION						\
	-DSHAREDIR=\""$(datadir)"\"					\
	-I$(top_srcdir)/src						\
	$(WARN_CFLAGS)							\
	$(GLIB2_CFLAGS)							\
	$(GCOV_CFLAGS)							\
	$(DBUS_CFLAGS)							\
	$(SQLITE3_CFLAGS)

noinst_LTLIBRARIES = libtracker-db.la

libtracker_db_la_SOURCES = 						\
	tracker-db-dbus.c						\
	tracker-db-interface.c						\
	tracker-db-interface-sqlite.c					\
	tracker-db-manager.c						\
	tracker-db-journal.c						\
	tracker-db-config.c

noinst_HEADERS =							\
	tracker-db.h							\
	tracker-db-dbus.h						\
	tracker-db-interface.h						\
	tracker-db-interface-sqlite.h					\
	tracker-db-manager.h 						\
	tracker-db-journal.h						\
	tracker-db-config.h

libtracker_db_la_LIBADD = 						\
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(SQLITE3_LIBS)							\
	$(DBUS_LIBS)							\
	$(GCOV_LIBS)							\
	$(GLIB2_LIBS)							\
	-lz								\
	-lm

if HAVE_TRACKER_FTS
libtracker_db_la_LIBADD += 						\
	$(top_builddir)/src/libtracker-fts/libtracker-fts.la
endif

EXTRA_DIST = libtracker-db.vapi

