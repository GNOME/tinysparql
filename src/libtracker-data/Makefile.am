include $(top_srcdir)/Makefile.decl

INCLUDES =								\
	-DSHAREDIR=\""$(datadir)"\"					\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-DTRACKER_COMPILATION						\
	-I$(top_srcdir)/src						\
	-I$(top_builddir)/src						\
	-I$(top_builddir)/src/libtracker-sparql				\
	$(WARN_CFLAGS)							\
	$(GLIB2_CFLAGS)							\
	$(DBUS_CFLAGS)							\
	$(UUID_CFLAGS)							\
	$(GCOV_CFLAGS)							\
	$(SQLITE3_CFLAGS)

BUILT_SOURCES = libtracker-data.vala.stamp

libtracker_datadir = $(libdir)/tracker-$(TRACKER_API_VERSION)
libtracker_data_LTLIBRARIES = libtracker-data.la

libtracker_data_la_VALASOURCES = 					\
	tracker-sparql-expression.vala					\
	tracker-sparql-pattern.vala					\
	tracker-sparql-query.vala					\
	tracker-sparql-scanner.vala					\
	tracker-turtle-reader.vala

libtracker_data_la_SOURCES = 						\
	tracker-class.c							\
	tracker-data-backup.c						\
	tracker-data-manager.c						\
	tracker-data-query.c						\
	tracker-data-update.c						\
	tracker-db-config.c						\
	tracker-db-dbus.c						\
	tracker-db-interface.c						\
	tracker-db-interface-sqlite.c					\
	tracker-db-manager.c						\
	tracker-db-journal.c						\
	tracker-namespace.c						\
	tracker-ontology.c						\
	tracker-ontologies.c						\
	tracker-property.c						\
	libtracker-data.vala.stamp					\
	$(libtracker_data_la_VALASOURCES:.vala=.c)

noinst_HEADERS =							\
	tracker-class.h							\
	tracker-data.h							\
	tracker-data-backup.h						\
	tracker-data-manager.h						\
	tracker-data-query.h						\
	tracker-data-update.h						\
	tracker-db-config.h						\
	tracker-db-dbus.h						\
	tracker-db-interface.h						\
	tracker-db-interface-sqlite.h					\
	tracker-db-manager.h 						\
	tracker-db-journal.h						\
	tracker-namespace.h						\
	tracker-ontology.h						\
	tracker-ontologies.h						\
	tracker-property.h						\
	tracker-sparql-query.h

vapi_sources =								\
	$(top_srcdir)/src/vapi/posix.vapi				\
	$(top_srcdir)/src/vapi/glib-2.0-fixes.vapi			\
	$(top_srcdir)/src/libtracker-sparql/tracker-sparql-$(TRACKER_API_VERSION).vapi	\
	$(top_srcdir)/src/libtracker-common/libtracker-common.vapi	\
	libtracker-data.vapi

libtracker-data.vala.stamp: $(libtracker_data_la_VALASOURCES) $(vapi_sources)
	$(AM_V_GEN)$(VALAC) $(GCOV_VALAFLAGS) -C $(VALAFLAGS) -H tracker-sparql-query.h --vapi tracker-sparql-query.vapi --pkg gio-2.0 $^
	touch $@

libtracker_data_la_LDFLAGS =						\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)

libtracker_data_la_LIBADD = 						\
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(top_builddir)/src/libtracker-sparql/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(SQLITE3_LIBS)							\
	$(DBUS_LIBS)							\
	$(GLIB2_LIBS)							\
	$(UUID_LIBS)							\
	$(GCOV_LIBS)							\
	-lz								\
	-lm

if HAVE_TRACKER_FTS
libtracker_data_la_LIBADD += 						\
	$(top_builddir)/src/libtracker-fts/libtracker-fts.la
endif

MAINTAINERCLEANFILES =							\
	libtracker-data.vala.stamp					\
	$(libtracker_data_la_VALASOURCES:.vala=.c)			\
	tracker-sparql-query.h						\
	$(NULL)

EXTRA_DIST = $(libtracker_data_la_VALASOURCES)				\
	libtracker-data.vala.stamp					\
	libtracker-data.vapi						\
	tracker-sparql-query.vapi

