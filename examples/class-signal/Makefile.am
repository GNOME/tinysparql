include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = class-signal

AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	$(LIBTRACKER_SPARQL_CFLAGS)                    \
	$(TRACKER_DBUS_CFLAGS)

LDADD =                                                \
	$(top_builddir)/src/libtracker-sparql/libtracker-sparql-$(TRACKER_API_VERSION).la \
	$(BUILD_LIBS)                                  \
	$(LIBTRACKER_SPARQL_LIBS)                      \
	$(TRACKER_DBUS_LIBS)

# NOTE: class-signal needed to have gio/dbus/dbus-glib-1 CFLAGS/LIBS
# added. The reason for this is that the
# LIBTRACKER_SPARQL_{CFLAGS|LIBS} no longer include dbus/dbus-glib-1
# so these tests fail without that. The fix is to update the test
# cases to use gdbus. For now this is a temporary fix for these tests,
# -mr.

class_signal_SOURCES = class-signal.c
