AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-DLOCALEDIR=\""$(localedir)"\"                 \
	-DMANDIR=\""$(mandir)"\" \
	-DTRACKER_EXTRACTOR_RULES_DIR=\""$(TRACKER_EXTRACT_RULES_DIR)"\" \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	$(TRACKER_CFLAGS)

libs =                                                 \
	$(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-miner/libtracker-miner-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS)                                  \
	$(TRACKER_LIBS)

bin_PROGRAMS =                                         \
	tracker \
	tracker-import                                 \
	tracker-info                                   \
	tracker-search                                 \
	tracker-sparql                                 \
	tracker-stats                                  \
	tracker-tag

tracker_search_SOURCES = tracker-search.c
tracker_search_LDADD = $(libs)

tracker_stats_SOURCES = tracker-stats.c
tracker_stats_LDADD = $(libs)

tracker_tag_SOURCES = tracker-tag.c
tracker_tag_LDADD = $(libs)

tracker_info_SOURCES = tracker-info.c
tracker_info_LDADD = $(libs)

tracker_sparql_SOURCES = tracker-sparql.c
tracker_sparql_LDADD = $(libs)

tracker_import_SOURCES = tracker-import.c
tracker_import_LDADD = $(libs)

tracker_SOURCES = \
	tracker-main.c \
	tracker-config.c \
	tracker-config.h \
	tracker-daemon.c \
	tracker-daemon.h \
	tracker-dbus.c \
	tracker-dbus.h \
	tracker-help.c \
	tracker-help.h \
	tracker-index.c \
	tracker-index.h \
	tracker-process.c \
	tracker-process.h \
	tracker-reset.c \
	tracker-reset.h

tracker_LDADD = \
	$(top_builddir)/src/libtracker-control/libtracker-control-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-miner/libtracker-miner-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-data/libtracker-data.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS) \
	$(TRACKER_LIBS)
