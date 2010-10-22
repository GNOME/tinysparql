include $(top_srcdir)/Makefile.decl

INCLUDES =								\
	$(BUILD_CFLAGS)							\
	-DLOCALEDIR=\""$(localedir)"\" 					\
	-I$(top_srcdir)/src						\
	-I$(top_builddir)/src						\
	-I$(top_builddir)/src/libtracker-sparql				\
	-I$(top_srcdir)/src/libtracker-sparql				\
	$(DBUS_CFLAGS)							\
	$(GIO_CFLAGS)							\
	$(GTHREAD_CFLAGS)						\
	$(GLIB2_CFLAGS)

libs = 									\
	$(top_builddir)/src/libtracker-sparql/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(BUILD_LIBS)							\
	$(DBUS_LIBS)							\
	$(GIO_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GLIB2_LIBS)

noinst_PROGRAMS = mtp-dummy

mtp_dummy_SOURCES = mtp-dummy.c
mtp_dummy_LDADD = $(libs)