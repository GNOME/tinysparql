include $(top_srcdir)/Makefile.decl

if HAVE_LIBINOTIFY
libinotify_dir = libinotify
endif

SUBDIRS = 					\
	libstemmer				\
	$(libinotify_dir)			\
	libtracker-common 			\
	libtracker-client			\
	libtracker-fts				\
	libtracker-db				\
	libtracker-data				\
	libtracker-extract			\
	libtracker-miner			\
	miners					\
	plugins					\
	tracker-store				\
	tracker-control				\
	tracker-utils				\
	tracker-extract				\
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

if HAVE_MINER_TWITTER
SUBDIRS += tracker-miner-twitter
endif
