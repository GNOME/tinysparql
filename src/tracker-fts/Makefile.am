include $(top_srcdir)/Makefile.decl

INCLUDES =							\
	-I$(top_srcdir)/src					\
	-DTRACKER_COMPILATION					\
	-DNDEBUG						\
	$(WARN_CFLAGS)						\
	$(GLIB2_CFLAGS)						\
	$(GCOV_CFLAGS)						\
	$(PANGO_CFLAGS)						\
	$(SQLITE3_CFLAGS)

tracker_fts_modulesdir = $(libdir)/tracker-$(TRACKER_API_VERSION)
tracker_fts_modules_LTLIBRARIES = libtracker-fts.la

libtracker_fts_la_SOURCES = 					\
	tracker-fts.c						\
	tracker-fts.h						\
	tracker-fts-config.c					\
	tracker-fts-config.h					\
	tracker-fts-hash.c					\
	tracker-fts-hash.h

libtracker_fts_la_LDFLAGS =					\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)

libtracker_fts_la_LIBADD =						\
	$(SQLITE3_LIBS)						\
	$(GTHREAD_LIBS)						\
	$(GCOV_LIBS)						\
	$(PANGO_LIBS)						\
	$(GLIB2_LIBS)							
