include $(top_srcdir)/Makefile.decl

INCLUDES =								\
	-DSHAREDIR=\""$(datadir)"\"					\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-DTRACKER_COMPILATION						\
	-I$(top_srcdir)/src						\
	$(WARN_CFLAGS)							\
	$(GLIB2_CFLAGS)							\
	$(DBUS_CFLAGS)							\
	$(UUID_CFLAGS)							\
	$(GCOV_CFLAGS)

BUILT_SOURCES = libtracker-data.vala.stamp

libtracker_datadir = $(libdir)/tracker-$(TRACKER_API_VERSION)
libtracker_data_LTLIBRARIES = libtracker-data.la

libtracker_data_la_VALASOURCES = 					\
	tracker-sparql-query.vala					\
	tracker-sparql-scanner.vala					\
	tracker-turtle-reader.vala

libtracker_data_la_SOURCES = 						\
	tracker-data-backup.c						\
	tracker-data-manager.c						\
	tracker-data-query.c						\
	tracker-data-update.c						\
	libtracker-data.vala.stamp					\
	$(libtracker_data_la_VALASOURCES:.vala=.c)

noinst_HEADERS =							\
	tracker-data.h							\
	tracker-data-backup.h						\
	tracker-data-manager.h						\
	tracker-data-query.h						\
	tracker-data-update.h						\
	tracker-sparql-query.h

libtracker-data.vala.stamp: $(libtracker_data_la_VALASOURCES) ../libtracker-common/posix.vapi ../libtracker-common/libtracker-common.vapi libtracker-data.vapi ../libtracker-db/libtracker-db.vapi
	$(AM_V_GEN)$(VALAC) $(GCOV_VALAFLAGS) -C $(VALAFLAGS) -H tracker-sparql-query.h $^
	touch $@

libtracker_data_la_LDFLAGS =						\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)

libtracker_data_la_LIBADD = 						\
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(top_builddir)/src/libtracker-db/libtracker-db.la		\
	$(DBUS_LIBS)							\
	$(GLIB2_LIBS)							\
	$(UUID_LIBS)							\
	$(GCOV_LIBS)							\
	-lz

MAINTAINERCLEANFILES =							\
	libtracker-data.vala.stamp					\
	$(libtracker_data_la_VALASOURCES:.vala=.c)			\
	tracker-sparql-query.h					\
	$(NULL)

EXTRA_DIST = $(libtracker_data_la_VALASOURCES)				\
	libtracker-data.vala.stamp					\
	libtracker-data.vapi

