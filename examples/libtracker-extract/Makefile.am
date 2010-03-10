include $(top_srcdir)/Makefile.decl

module_flags = -module -avoid-version -no-undefined
modulesdir = $(libdir)/tracker-$(TRACKER_API_VERSION)/extract-modules

INCLUDES = 								\
	-I$(top_srcdir)/src 						\
	$(WARN_CFLAGS)							\
	$(GLIB2_CFLAGS)							\
	$(GCOV_CFLAGS)

noinst_LTLIBRARIES = libextract-mockup.la

libextract_mockup_la_SOURCES = tracker-extract-mockup.c
libextract_mockup_la_LDFLAGS = $(module_flags)
libextract_mockup_la_LIBADD = 						\
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(GLIB2_LIBS)  							\
	$(GCOV_LIBS)

