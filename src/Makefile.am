include $(top_srcdir)/Makefile.decl

if HAVE_INOTIFY
inotify_dir = libinotify
endif

SUBDIRS = 					\
	libstemmer				\
	$(inotify_dir)				\
	libtracker-common 			\
	libtracker-client			\
	libtracker-fts				\
	libtracker-db				\
	libtracker-data				\
	libtracker-extract			\
	libtracker-miner			\
	plugins					\
	tracker-store				\
	tracker-control				\
	tracker-utils				\
	tracker-extract				\
	tracker-miner-fs			\
	tracker-writeback			\
	vapi

if HAVE_TRACKER_PREFERENCES
SUBDIRS += tracker-preferences
endif

if HAVE_TRACKER_STATUS_ICON
SUBDIRS += tracker-status-icon
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

if USING_MINER_RSS
SUBDIRS += tracker-miner-rss
endif
