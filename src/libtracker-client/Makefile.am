include $(top_srcdir)/Makefile.decl

INCLUDES = 					\
	-DLOCALEDIR=\""$(localedir)"\" 		\
	-DG_LOG_DOMAIN=\"Tracker\"		\
	-I$(top_srcdir)/src			\
	$(WARN_CFLAGS)				\
	$(GLIB2_CFLAGS)				\
	$(GCOV_CFLAGS)				\
	$(DBUS_CFLAGS)			

lib_LTLIBRARIES = libtracker-client-@TRACKER_API_VERSION@.la

libtracker_clientincdir = $(includedir)/tracker-$(TRACKER_API_VERSION)/libtracker-client

libtracker_client_@TRACKER_API_VERSION@_la_SOURCES = tracker.c

libtracker_client_@TRACKER_API_VERSION@_la_LDFLAGS = 	\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-export-symbols-regex '^tracker_.*'

libtracker_client_@TRACKER_API_VERSION@_la_LIBADD =	\
	$(GLIB2_LIBS) 					\
	$(DBUS_LIBS)					\
	$(GCOV_LIBS)					\
	$(GOBJECT_LIBS)

libtracker_clientinc_HEADERS = 				\
	tracker.h

# Generate DBus files from XML data.
dbus_sources = 						\
	tracker-resources-glue.h			\
	tracker-statistics-glue.h

%-glue.h: $(top_srcdir)/data/dbus/%.xml
	$(AM_V_GEN)$(DBUSBINDINGTOOL) --mode=glib-client --output=$@ --prefix=$(subst -,_,$*) $^

BUILT_SOURCES = $(dbus_sources)

CLEANFILES = $(BUILT_SOURCES)
