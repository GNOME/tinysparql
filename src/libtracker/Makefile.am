INCLUDES =				\
	-DDATADIR=\""$(datadir)"\"	\
	$(GLIB2_CFLAGS)			\
	$(DBUS_CFLAGS)			\
	-g -W -Wall 

lib_LTLIBRARIES = libmetatracker.la

libmetatracker_la_LIBADD = $(GLIB2_LIBS) 	\
	    	       $(DBUS_LIBS)

libmetatracker_la_SOURCES = tracker.c
include_HEADERS = tracker.h tracker-client.h		

bin_PROGRAMS = tracker-search tracker-query tracker-meta-folder

tracker_search_SOURCES = tracker-search.c

tracker_search_LDADD = 	$(GLIB2_LIBS) 	\
		 	$(DBUS_LIBS)	\
			$(top_builddir)/src/libtracker/libmetatracker.la

tracker_query_SOURCES = tracker-query.c

tracker_query_LDADD = 	$(GLIB2_LIBS) 	\
		 	$(DBUS_LIBS)	\
			$(top_builddir)/src/libtracker/libmetatracker.la

tracker_meta_folder_SOURCES = tracker-get-meta-for-folder.c

tracker_meta_folder_LDADD = 	$(GLIB2_LIBS) 	\
		 		$(DBUS_LIBS)	\
				$(top_builddir)/src/libtracker/libmetatracker.la
 
install-exec-hook:
	ln -fs $(libdir)/libmetatracker.so $(libdir)/libtracker.so
