include $(top_srcdir)/Makefile.decl

SUBDIRS = 			\
	common 			\
	libtracker-common	\
	libtracker-client	\
	libtracker-extract	\
	libtracker-miner	\
	libtracker-data		\
	tracker-miner-fs	\
	tracker-extract		\
	tracker-steroids	\
	tracker-store		\
	tracker-writeback

if DIST_FUNCTIONAL_TESTS
SUBDIRS += functional-tests
endif

if HAVE_TRACKER_FTS
SUBDIRS += libtracker-fts
endif
