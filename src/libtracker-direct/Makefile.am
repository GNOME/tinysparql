include $(top_srcdir)/Makefile.decl

INCLUDES =								\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-DTRACKER_COMPILATION						\
	-I$(top_srcdir)/src						\
	-I$(top_srcdir)/src/libtracker-sparql				\
	-I$(top_srcdir)/src/libtracker-data				\
	-I$(top_builddir)/src						\
	$(WARN_CFLAGS)							\
	$(GLIB2_CFLAGS)							\
	$(GCOV_CFLAGS)

lib_LTLIBRARIES = libtracker-direct-@TRACKER_API_VERSION@.la

libtracker_directincludedir = $(includedir)/tracker-$(TRACKER_API_VERSION)/libtracker-direct

libtracker_direct_la_VALASOURCES = \
	tracker-direct.vala

libtracker_direct_@TRACKER_API_VERSION@_la_SOURCES = 	\
	libtracker-direct.vala.stamp			\
	$(libtracker_direct_la_VALASOURCES:.vala=.c)

libtracker_direct_@TRACKER_API_VERSION@_la_LDFLAGS = 	\
	-shared -fPIC -module -avoid-version

#	-export-symbols-regex '^tracker_.*'

libtracker_directinclude_HEADERS = 			\
	$(top_srcdir)/src/libtracker-sparql/tracker-sparql-$(TRACKER_API_VERSION).vapi	\
	tracker-direct.h

# Vala sources
vapi_sources =						\
	$(top_srcdir)/src/libtracker-sparql/tracker-sparql-$(TRACKER_API_VERSION).vapi	\
	$(top_srcdir)/src/libtracker-common/libtracker-common.vapi	\
	$(top_srcdir)/src/libtracker-data/libtracker-data.vapi	\
	$(top_srcdir)/src/libtracker-data/tracker-sparql-query.vapi

# Vala bindings
vapidir = $(datadir)/vala/vapi
vapi_DATA = \
	tracker-direct-$(TRACKER_API_VERSION).vapi

libtracker-direct.vala.stamp: $(libtracker_direct_la_VALASOURCES) $(vapi_sources)
	$(AM_V_GEN)$(VALAC) $(GCOV_VALAFLAGS) -C $(VALAFLAGS) --pkg gio-2.0 -H tracker-direct.h --vapi tracker-direct-$(TRACKER_API_VERSION).vapi $^
	$(AM_V_GEN)touch $@

libtracker_direct_@TRACKER_API_VERSION@_la_LIBADD = 						\
	$(top_builddir)/src/libtracker-data/libtracker-data.la	\
	$(GLIB2_LIBS)

BUILT_SOURCES = 					\
	libtracker-direct.vala.stamp

CLEANFILES = 						\
	$(BUILT_SOURCES)

MAINTAINERCLEANFILES =					\
	$(libtracker_direct_la_VALASOURCES:.vala=.c) \
	libtracker-direct.vala.stamp			\
	tracker-direct.h

EXTRA_DIST = 						\
	$(libtracker_direct_la_VALASOURCES) \
	libtracker-direct.vala.stamp
