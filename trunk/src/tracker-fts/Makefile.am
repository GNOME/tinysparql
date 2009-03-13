include $(top_srcdir)/Makefile.decl

INCLUDES =							\
	-I$(top_srcdir)/src					\
	$(WARN_CFLAGS)						\
	$(GLIB2_CFLAGS)						\
	$(GCOV_CFLAGS)						\
	$(PANGO_CFLAGS)						\
	$(SQLITE3_CFLAGS)					\
	-DSTORE_CATEGORY=1			

module_flags = -module -avoid-version -no-undefined

tracker_fts_modulesdir = $(libdir)
tracker_fts_modules_LTLIBRARIES = tracker-fts.la

tracker_fts_la_SOURCES = 					\
	tracker-fts.c						\
	tracker-fts-hash.c				

tracker_fts_la_LDFLAGS = $(module_flags)

tracker_fts_la_LIBADD =						\
	$(top_builddir)/src/libtracker-db/libtracker-db.la	\
	$(SQLITE3_LIBS)						\
	$(GTHREAD_LIBS)						\
	$(GCOV_LIBS)						\
	$(PANGO_LIBS)						\
	$(GLIB2_LIBS)							
