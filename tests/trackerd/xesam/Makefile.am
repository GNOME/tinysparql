include $(top_srcdir)/Makefile.decl

if HAVE_XESAM_GLIB

noinst_LTLIBRARIES = libxesamhelper.la

INCLUDES = \
        -DG_LOG_DOMAIN=\"Tracker\" \
        -I$(top_srcdir)/src \
        $(GMODULE_CFLAGS) \
        $(GTHREAD_CFLAGS) \
        $(GLIB2_CFLAGS) \
	$(XESAM_GLIB_CFLAGS)

libxesamhelper_la_SOURCES = \
	xesam-g-debug-private.h \
	xesam-g-utils.h \
	xesam-g-globals-private.h \
	gtestextensions.c \
	gtestextensions.h \
	xesam-g-test-query-builder.c \
	xesam-g-test-query-builder.h \
	xesam-g-testsearcher.c \
	xesam-g-testsearcher.h

endif
