include $(top_srcdir)/Makefile.decl

SUBDIRS = 					\
	libstemmer				\
	libinotify				\
	libtracker-common 			\
	tracker-fts				\
	libtracker-db				\
	libtracker-data				\
	libtracker-client			\
	libtracker-miner			\
	plugins					\
	tracker-store				\
	tracker-utils				\
	tracker-extract				\
	tracker-miner-fs

if HAVE_TRACKER_PREFERENCES
SUBDIRS += tracker-preferences
endif

if HAVE_TRACKER_STATUS_ICON
SUBDIRS += tracker-status-icon
endif

if HAVE_LIBTRACKERGTK
SUBDIRS += libtracker-gtk
endif

if HAVE_TRACKER_SEARCH_TOOL
SUBDIRS += tracker-search-tool
endif

if HAVE_TRACKER_EXPLORER
SUBDIRS += tracker-explorer
endif

DIST_SUBDIRS = 					\
	libstemmer				\
	libinotify				\
	libtracker-common 			\
	tracker-fts				\
	libtracker-db				\
	libtracker-data				\
	libtracker-client			\
	libtracker-miner			\
	tracker-store				\
	tracker-utils				\
	tracker-explorer			\
	tracker-extract				\
	tracker-miner-fs			\
	libtracker-gtk				\
	tracker-status-icon 			\
	tracker-search-tool			\
	tracker-preferences			\
	plugins
