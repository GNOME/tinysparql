if HAVE_TRACKER_FTS
libtrackerfts_dir = libtracker-fts
endif

SUBDIRS =                                              \
	ontologies				       \
	gvdb                                           \
	libtracker-common                              \
	libtracker-sparql                              \
	$(libtrackerfts_dir)                           \
	libtracker-bus                                 \
	libtracker-data                                \
	libtracker-direct                              \
	libtracker-remote                              \
	libtracker-sparql-backend                      \
	libtracker-extract                             \
	libtracker-miner                               \
	libtracker-control                             \
	tracker                                        \
	miners                                         \
	tracker-store

if HAVE_TRACKER_EXTRACT
SUBDIRS += tracker-extract
endif

if HAVE_TRACKER_WRITEBACK
SUBDIRS += tracker-writeback
endif

EXTRA_DIST = \
	meson.build \
	create-miner-symlinks.sh \
	install-generated-header.sh
