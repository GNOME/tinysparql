include $(top_srcdir)/Makefile.decl

if HAVE_TRACKER_FTS
libstemmer_dir = libstemmer
endif

if HAVE_TRACKER_FTS
libtrackerfts_dir = libtracker-fts
endif

SUBDIRS =                                              \
	gvdb                                           \
	$(libstemmer_dir)                              \
	libtracker-common                              \
	libtracker-sparql                              \
	$(libtrackerfts_dir)                           \
	libtracker-bus                                 \
	libtracker-data                                \
	libtracker-direct                              \
	libtracker-sparql-backend                      \
	libtracker-extract                             \
	libtracker-miner                               \
	miners                                         \
	plugins                                        \
	tracker-store                                  \
	tracker-control                                \
	tracker-utils                                  \
	tracker-extract                                \
	tracker-writeback                              \
	vapi

if HAVE_TRACKER_PREFERENCES
SUBDIRS += tracker-preferences
endif

if HAVE_TRACKER_NEEDLE
SUBDIRS += tracker-needle
endif

if HAVE_TRACKER_SEARCH_BAR
SUBDIRS += tracker-search-bar
endif

if HAVE_TRACKER_EXPLORER
SUBDIRS += tracker-explorer
endif
