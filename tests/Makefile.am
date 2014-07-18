include $(top_srcdir)/Makefile.decl

SUBDIRS =                                              \
	common                                         \
	gvdb					       \
	libtracker-common                              \
	libtracker-miner                               \
	libtracker-data                                \
	libtracker-sparql                              \
	tracker-steroids

if HAVE_TRACKER_FTS
SUBDIRS += libtracker-fts
endif

if HAVE_TRACKER_EXTRACT
SUBDIRS += libtracker-extract
endif

if HAVE_TRACKER_WRITEBACK
SUBDIRS += tracker-writeback
endif

if DIST_FUNCTIONAL_TESTS
SUBDIRS += functional-tests
endif
