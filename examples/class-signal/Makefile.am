include $(top_srcdir)/Makefile.decl

INCLUDES = \
	-I$(top_srcdir)/src/libtracker-sparql \
	-I$(top_srcdir)/src \
	$(WARN_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(GLIB2_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GTHREAD_CFLAGS)

noinst_PROGRAMS = class-signal

class_signal_SOURCES = class-signal.c

class_signal_LDADD = \
	$(top_builddir)/src/libtracker-sparql/libtracker-sparql-$(TRACKER_API_VERSION).la \
	$(GIO_LIBS) \
	$(GLIB2_LIBS) \
	$(DBUS_LIBS)
