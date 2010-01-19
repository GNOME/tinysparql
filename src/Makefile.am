include $(top_srcdir)/Makefile.decl

SUBDIRS = 					\
	libstemmer				\
	libinotify				\
	libtracker-common 			\
	libtracker-fts				\
	libtracker-db				\
	libtracker-data				\
	libtracker-extract			\
	libtracker-client			\
	libtracker-miner			\
	plugins					\
	tracker-store				\
	tracker-control				\
	tracker-utils				\
	tracker-extract				\
	tracker-miner-fs			\
	tracker-writeback

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

if HAVE_TRACKER_SEARCH_BAR
SUBDIRS += tracker-search-bar
endif

if HAVE_TRACKER_EXPLORER
SUBDIRS += tracker-explorer
endif

