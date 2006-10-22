INCLUDES =				\
	-DDATADIR=\""$(datadir)"\"	\
	$(GLIB2_CFLAGS)			\
	$(DBUS_CFLAGS)			\
	$(CFLAGS) 			\
	-g

lib_LTLIBRARIES = libtrackerclient.la

libtrackerclient_la_LIBADD = $(GLIB2_LIBS) 	\
	    	       $(DBUS_LIBS)

libtrackerclient_la_SOURCES = tracker.c
include_HEADERS = tracker.h tracker-client.h		

bin_PROGRAMS = tracker-search tracker-query tracker-meta-folder tracker-stats tracker-tag tracker-files

tracker_search_SOURCES = tracker-search.c

tracker_search_LDADD = 	$(GLIB2_LIBS) 	\
		 	$(DBUS_LIBS)	\
		 	$(GOBJECT_LIBS)	\
			$(top_builddir)/src/libtracker/libtrackerclient.la

tracker_query_SOURCES = tracker-query.c

tracker_query_LDADD = 	$(GLIB2_LIBS) 	\
		 	$(DBUS_LIBS)	\
		 	$(GOBJECT_LIBS)	\
			$(top_builddir)/src/libtracker/libtrackerclient.la

tracker_meta_folder_SOURCES = tracker-get-meta-for-folder.c

tracker_meta_folder_LDADD = 	$(GLIB2_LIBS) 	\
		 		$(DBUS_LIBS)	\
		 		$(GOBJECT_LIBS)	\
				$(top_builddir)/src/libtracker/libtrackerclient.la

tracker_stats_SOURCES = tracker-stats.c

tracker_stats_LDADD = 	$(GLIB2_LIBS) 	\
		 	$(DBUS_LIBS)	\
		 	$(GOBJECT_LIBS)	\
			$(top_builddir)/src/libtracker/libtrackerclient.la

tracker_tag_SOURCES = tracker-tag.c

tracker_tag_LDADD = 	$(GLIB2_LIBS) 	\
		 	$(DBUS_LIBS)	\
		 	$(GOBJECT_LIBS)	\
			$(top_builddir)/src/libtracker/libtrackerclient.la

tracker_files_SOURCES = tracker-files.c

tracker_files_LDADD = 	$(GLIB2_LIBS) 	\
		 	$(DBUS_LIBS)	\
		 	$(GOBJECT_LIBS)	\
			$(top_builddir)/src/libtracker/libtrackerclient.la
 
install-exec-hook:
	cd $(DESTDIR)/$(libdir) && rm -f libtracker.so && ln -s libtrackerclient.so libtracker.so
	cd $(DESTDIR)/$(libdir) && rm -f libtracker.so.0 && ln -s libtrackerclient.so.0 libtracker.so.0
