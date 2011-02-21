include $(top_srcdir)/Makefile.decl

SUBDIRS =                                              \
	gtk-sparql                                     \
	lyrics                                         \
	services                                       \
	data-generators                                \
	mtp                                            \
	tracker-sql

if HAVE_TOTEM_PL_PARSER
SUBDIRS += playlists
endif

if HAVE_TRACKER_RESDUMP
SUBDIRS += tracker-resdump
endif
