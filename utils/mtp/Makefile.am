include $(top_srcdir)/Makefile.decl

INCLUDES =								\
	-DLOCALEDIR=\""$(localedir)"\" 					\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-DTRACKER_COMPILATION						\
	-I$(top_srcdir)/src						\
	-I$(top_builddir)/src						\
	-I$(top_builddir)/src/libtracker-sparql				\
	-I$(top_srcdir)/src/libtracker-sparql				\
	$(WARN_CFLAGS)							\
	$(GCOV_CFLAGS)							\
	$(DBUS_CFLAGS)							\
	$(GIO_CFLAGS)							\
	$(GTHREAD_CFLAGS)						\
	$(GLIB2_CFLAGS)

libs = 									\
	$(top_builddir)/src/libtracker-sparql/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(DBUS_LIBS)							\
	$(GIO_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GLIB2_LIBS)							\
	$(GCOV_LIBS)

noinst_PROGRAMS = mtp-dummy

mtp_dummy_SOURCES = mtp-dummy.c
mtp_dummy_LDADD = $(libs)