include $(top_srcdir)/Makefile.decl

module_flags = -module -avoid-version -no-undefined
modulesdir = $(libdir)/tracker-$(TRACKER_API_VERSION)/extract-modules

INCLUDES = 								\
	-DLOCALEDIR=\""$(localedir)"\" 					\
	-DMODULESDIR=\"$(modulesdir)\"					\
	-I$(top_srcdir)/src 						\
	$(WARN_CFLAGS)							\
	$(GLIB2_CFLAGS)							\
	$(GCOV_CFLAGS)

modules_LTLIBRARIES = libextract-mockup.la

libextract_mockup_la_SOURCES = tracker-extract-mockup.c
libextract_mockup_la_LDFLAGS = $(module_flags)
libextract_mockup_la_LIBADD = 						\
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(GLIB2_LIBS)  							\
	$(GCOV_LIBS)

