include $(top_srcdir)/Makefile.decl

INCLUDES =								\
	-I$(top_srcdir)/src						\
	-DTRACKER_COMPILATION						\
	-DNDEBUG							\
	$(WARN_CFLAGS)							\
	$(GLIB2_CFLAGS)							\
	$(GCOV_CFLAGS)							\
	$(PANGO_CFLAGS)							\
	$(DBUS_CFLAGS)							\
	$(UNAC_CFLAGS)							\
	$(SQLITE3_CFLAGS)

noinst_LTLIBRARIES = libtracker-fts.la

libtracker_fts_la_SOURCES = 						\
	tracker-fts.c							\
	tracker-fts.h							\
	tracker-fts-config.c						\
	tracker-fts-config.h						\
	tracker-fts-hash.c						\
	tracker-fts-hash.h						\
	tracker-parser.c						\
	tracker-parser.h

libtracker_fts_la_LIBADD =						\
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(SQLITE3_LIBS)							\
	$(DBUS_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GCOV_LIBS)							\
	$(PANGO_LIBS)							\
	$(UNAC_LIBS)							\
	$(GLIB2_LIBS)							
