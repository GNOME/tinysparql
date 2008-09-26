include $(top_srcdir)/Makefile.decl

SUBDIRS = xesam .

noinst_PROGRAMS = $(TEST_PROGS)

INCLUDES = \
	-DG_LOG_DOMAIN=\"Tracker\" \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/trackerd \
	-I$(top_srcdir)/tests/common	\
	$(GMODULE_CFLAGS) \
	$(GTHREAD_CFLAGS) \
	$(GLIB2_CFLAGS) \
	$(QDBM_CFLAGS) \
	$(GIO_GFLAGS) \
	$(DBUS_CFLAGS)

if HAVE_XESAM_GLIB

TEST_PROGS += tracker-xesam 

INCLUDES += $(XESAM_GLIB_CFLAGS)

tracker_xesam_SOURCES = \
	tracker-xesam-test.c \
	tracker-xesam-test.h \
	tracker-xesam-session-test.c \
	tracker-xesam-session-test.h \
	tracker-xesam-hit-test.c \
	tracker-xesam-hit-test.h \
	tracker-xesam-hits-test.c \
	tracker-xesam-hits-test.h

tracker_xesam_LDADD = \
	$(top_builddir)/tests/trackerd/xesam/libxesamhelper.la \
	$(XESAM_GLIB_LIBS) \
	$(GMODULE_LIBS) \
	$(GTHREAD_LIBS) \
	$(GLIB2_LIBS) \
	$(DBUS_LIBS)

endif

