include $(top_srcdir)/Makefile.decl

if HAVE_INOTIFY
build_libinotify = libinotify
endif

if HAVE_TRACKER_PREFERENCES
build_tracker_preferences = tracker-preferences
endif

if HAVE_TRACKER_APPLET
build_tracker_applet = tracker-applet
endif

if HAVE_LIBTRACKERGTK
build_libtrackergtk = libtracker-gtk
endif

if HAVE_TRACKER_SEARCH_TOOL
build_tracker_search_tool = tracker-search-tool
endif

SUBDIRS = 					\
	libstemmer				\
	$(build_libinotify)			\
	rasqal		 			\
	libtracker-common 			\
	libtracker-db				\
	libtracker-data				\
	libtracker				\
	tracker-fts				\
	plugins					\
	trackerd				\
	tracker-utils				\
	tracker-extract				\
	tracker-indexer				\
	$(build_libtrackergtk)			\
	$(build_tracker_applet)			\
	$(build_tracker_search_tool)		\
	$(build_tracker_preferences)

DIST_SUBDIRS = 					\
	libstemmer				\
	libinotify				\
	rasqal		 			\
	libtracker-common 			\
	libtracker-db				\
	libtracker-data				\
	libtracker				\
	tracker-fts				\
	trackerd				\
	tracker-utils				\
	tracker-extract				\
	tracker-indexer				\
	libtracker-gtk				\
	tracker-applet	 			\
	tracker-search-tool			\
	tracker-preferences			\
	plugins
