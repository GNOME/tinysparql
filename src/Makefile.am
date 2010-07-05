include $(top_srcdir)/Makefile.decl

if HAVE_TRACKER_FTS
libstemmer_dir = libstemmer
endif

if HAVE_TRACKER_FTS
libtrackerfts_dir = libtracker-fts
endif


SUBDIRS = 					\
	$(libstemmer_dir)			\
	libtracker-common 			\
	libtracker-sparql 			\
	libtracker-client			\
	$(libtrackerfts_dir)			\
	libtracker-bus				\
	libtracker-data				\
	libtracker-direct			\
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
