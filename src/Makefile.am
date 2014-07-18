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
	libtracker-control                             \
	libtracker-data                                \
	libtracker-direct                              \
	libtracker-sparql-backend                      \
	libtracker-extract                             \
	libtracker-miner                               \
	miners                                         \
	plugins                                        \
	tracker-store                                  \
	tracker-control                                \
	tracker-utils

if HAVE_TRACKER_EXTRACT
SUBDIRS += tracker-extract
endif

if HAVE_TRACKER_WRITEBACK
SUBDIRS += tracker-writeback
endif

if HAVE_TRACKER_PREFERENCES
SUBDIRS += tracker-preferences
endif

if HAVE_TRACKER_NEEDLE
SUBDIRS += tracker-needle
endif
