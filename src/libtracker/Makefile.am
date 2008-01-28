INCLUDES =				\
	-DTRACKER_DATADIR=\""$(datadir)"\"	\
	-DTRACKER_LOCALEDIR=\""$(localedir)"\" \
	$(GLIB2_CFLAGS)			\
	$(DBUS_CFLAGS)			

BUILT_SOURCES = tracker-client.h

lib_LTLIBRARIES = libtrackerclient.la

libtrackerclient_la_LIBADD = $(GLIB2_LIBS) 	\
	    	       	     $(DBUS_LIBS)	\
			     $(GOBJECT_LIBS)

libtrackerclient_la_SOURCES = tracker.c
libtrackerclient_la_LDFLAGS = -version-info 0:0:0

tracker-client.h: $(top_srcdir)/data/tracker-introspect.xml Makefile.am
	dbus-binding-tool --prefix=client_object --mode=glib-client --output=tracker-client.h $(top_srcdir)/data/tracker-introspect.xml

include_HEADERS = tracker.h tracker-client.h		

bin_PROGRAMS = tracker-search tracker-query tracker-meta-folder tracker-stats tracker-tag tracker-files tracker-status

tracker_search_SOURCES = tracker-search.c

tracker_search_LDADD = 	$(GLIB2_LIBS) 	\
		 	$(DBUS_LIBS)	\
		 	$(GOBJECT_LIBS)	\
			libtrackerclient.la

tracker_query_SOURCES = tracker-query.c

tracker_query_LDADD = 	$(GLIB2_LIBS) 	\
		 	$(DBUS_LIBS)	\
		 	$(GOBJECT_LIBS)	\
			libtrackerclient.la

tracker_meta_folder_SOURCES = tracker-get-meta-for-folder.c

tracker_meta_folder_LDADD = 	$(GLIB2_LIBS) 	\
		 		$(DBUS_LIBS)	\
		 		$(GOBJECT_LIBS)	\
				libtrackerclient.la

tracker_stats_SOURCES = tracker-stats.c

tracker_stats_LDADD = 	$(GLIB2_LIBS) 	\
		 	$(DBUS_LIBS)	\
		 	$(GOBJECT_LIBS)	\
			libtrackerclient.la

tracker_status_SOURCES = tracker-status.c

tracker_status_LDADD =   $(GLIB2_LIBS)   \
                       $(DBUS_LIBS)    \
                       $(GOBJECT_LIBS) \
                       libtrackerclient.la


tracker_tag_SOURCES = tracker-tag.c

tracker_tag_LDADD = 	$(GLIB2_LIBS) 	\
		 	$(DBUS_LIBS)	\
		 	$(GOBJECT_LIBS)	\
			libtrackerclient.la

tracker_files_SOURCES = tracker-files.c

tracker_files_LDADD = 	$(GLIB2_LIBS) 	\
		 	$(DBUS_LIBS)	\
		 	$(GOBJECT_LIBS)	\
			libtrackerclient.la
 
CLEANFILES = $(BUILT_SOURCES)
