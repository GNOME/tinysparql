include $(top_srcdir)/Makefile.decl

module_flags = -module -avoid-version -no-undefined
tracker_fts_modulesdir = $(libdir)

INCLUDES =								\
	-g								\
	-I$(top_srcdir)/src						\
	$(GLIB2_CFLAGS)							\
	$(SQLITE3_CFLAGS)						\
	$(PANGO_CFLAGS)

tracker_fts_modules_LTLIBRARIES = tracker-fts.la
		

tracker_fts_la_SOURCES = 						\
			tracker-fts.c					\
			tracker-fts-hash.c				
						
			
tracker_fts_la_LDFLAGS = $(module_flags)

tracker_fts_la_LIBADD =							\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(SQLITE3_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(PANGO_LIBS)							\
	$(GLIB2_LIBS)							
