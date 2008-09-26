include $(top_srcdir)/Makefile.decl

INCLUDES = 					\
	-DLOCALEDIR=\""$(localedir)"\" 		\
	-DG_LOG_DOMAIN=\"Tracker\"		\
	-I$(top_srcdir)/src			\
	$(GLIB2_CFLAGS)				\
	$(DBUS_CFLAGS)			

lib_LTLIBRARIES = libtrackerclient.la

libtrackerclient_la_SOURCES = tracker.c
libtrackerclient_la_LDFLAGS = -version-info 0:0:0
libtrackerclient_la_LIBADD = 			\
	$(GLIB2_LIBS) 				\
	$(DBUS_LIBS)				\
	$(GOBJECT_LIBS)
	
include_HEADERS = 				\
	tracker.h				\
	$(BUILT_SOURCES)		

# Generate DBus files from XML data.
dbus_sources = 					\
	tracker-daemon-glue.h			\
	tracker-files-glue.h			\
	tracker-keywords-glue.h			\
	tracker-metadata-glue.h			\
	tracker-search-glue.h

%-glue.h: $(top_srcdir)/data/dbus/%.xml
	$(DBUSBINDINGTOOL) --mode=glib-client --output=$@ --prefix=$(subst -,_,$*) $^

BUILT_SOURCES = $(dbus_sources)

CLEANFILES = $(BUILT_SOURCES)
