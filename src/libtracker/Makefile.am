include $(top_srcdir)/Makefile.decl

INCLUDES = 					\
	-DLOCALEDIR=\""$(localedir)"\" 		\
	-DG_LOG_DOMAIN=\"Tracker\"		\
	-I$(top_srcdir)/src			\
	$(WARN_CFLAGS)				\
	$(GLIB2_CFLAGS)				\
	$(GCOV_CFLAGS)				\
	$(DBUS_CFLAGS)			

lib_LTLIBRARIES = libtrackerclient-@TRACKER_API_VERSION@.la

libtrackerclient_@TRACKER_API_VERSION@_la_SOURCES = tracker.c

libtrackerclient_@TRACKER_API_VERSION@_la_LDFLAGS = 			\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)

libtrackerclient_@TRACKER_API_VERSION@_la_LIBADD = 			\
	$(GLIB2_LIBS) 				\
	$(DBUS_LIBS)				\
	$(GCOV_LIBS)				\
	$(GOBJECT_LIBS)

include_HEADERS = 				\
	tracker.h

# Generate DBus files from XML data.
dbus_sources = 					\
	tracker-daemon-glue.h			\
	tracker-search-glue.h

%-glue.h: $(top_srcdir)/data/dbus/%.xml
	$(DBUSBINDINGTOOL) --mode=glib-client --output=$@ --prefix=$(subst -,_,$*) $^

BUILT_SOURCES = $(dbus_sources)

CLEANFILES = $(BUILT_SOURCES)
