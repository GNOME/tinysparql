include $(top_srcdir)/Makefile.decl

if HAVE_INOTIFY
build_libinotify = libinotify
endif

if !HAVE_EXTERNAL_QDBM
build_qdbm = qdbm
endif

if ENABLE_SQLITE_FTS
build_sqlite_fts = tracker-fts
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
	$(build_qdbm)				\
	$(build_libinotify)			\
	libtracker-common 			\
	libtracker-db				\
	libtracker-data				\
	libtracker				\
	$(build_sqlite_fts)			\
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
	qdbm					\
	libinotify				\
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
