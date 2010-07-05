include $(top_srcdir)/Makefile.decl

lib_LTLIBRARIES = libtracker-direct-@TRACKER_API_VERSION@.la

libtracker_directincludedir = $(includedir)/tracker-$(TRACKER_API_VERSION)/libtracker-direct

libtracker_direct_@TRACKER_API_VERSION@_la_VALASOURCES = \
	tracker-direct.vala

libtracker_direct_@TRACKER_API_VERSION@_la_SOURCES = 	\
	libtracker-direct.vala.stamp			\
	$(libtracker_direct_@TRACKER_API_VERSION@_la_VALASOURCES:.vala=.c)

libtracker_direct_@TRACKER_API_VERSION@_la_LDFLAGS = 	\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) 

#	-export-symbols-regex '^tracker_.*'

libtracker_directinclude_HEADERS = 			\
	$(top_srcdir)/src/libtracker-sparql/tracker-sparql-$(TRACKER_API_VERSION).vapi	\
	tracker-connection.h

# Vala sources
vapi_sources =						\
	$(top_srcdir)/src/libtracker-sparql/tracker-sparql-$(TRACKER_API_VERSION).vapi	\
	$(top_srcdir)/src/vapi/posix.vapi		\
	$(top_srcdir)/src/vapi/glib-2.0-fixes.vapi

libtracker-direct.vala.stamp: $(libtracker_direct_@TRACKER_API_VERSION@_la_VALASOURCES) $(vapi_sources)
	$(AM_V_GEN)$(VALAC) $(GCOV_VALAFLAGS) -C $(VALAFLAGS) --pkg gio-2.0 -H tracker-connection.h $^
	$(AM_V_GEN)touch $@

BUILT_SOURCES = 					\
	libtracker-direct.vala.stamp

CLEANFILES = 						\
	$(BUILT_SOURCES)

MAINTAINERCLEANFILES =					\
	$(libtracker_direct_@TRACKER_API_VERSION@_la_VALASOURCES:.vala=.c) \
	libtracker-direct.vala.stamp			\
	tracker-connection.h

EXTRA_DIST = 						\
	$(libtracker_direct_@TRACKER_API_VERSION@_la_VALASOURCES) \
	libtracker-direct.vala.stamp
