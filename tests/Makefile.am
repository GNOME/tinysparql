include $(top_srcdir)/Makefile.decl

SUBDIRS =                                              \
	common

if HAVE_TRACKER_EXTRACT
SUBDIRS += libtracker-extract
endif

if HAVE_TRACKER_WRITEBACK
SUBDIRS += tracker-writeback
endif
