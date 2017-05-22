AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-DLIBEXECDIR=\""$(libexecdir)"\"               \
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

bin_PROGRAMS = tracker

tracker_SOURCES = \
	tracker-main.c \
	tracker-config.c \
	tracker-config.h \
	tracker-color.h \
	tracker-daemon.c \
	tracker-daemon.h \
	tracker-dbus.c \
	tracker-dbus.h \
	tracker-extract.c \
	tracker-extract.h \
	tracker-help.c \
	tracker-help.h \
	tracker-index.c \
	tracker-index.h \
	tracker-info.c \
	tracker-info.h \
	tracker-process.c \
	tracker-process.h \
	tracker-reset.c \
	tracker-reset.h \
	tracker-search.c \
	tracker-search.h \
	tracker-sparql.c \
	tracker-sparql.h \
	tracker-sql.c \
	tracker-sql.h \
	tracker-status.c \
	tracker-status.h \
	tracker-tag.c \
	tracker-tag.h

tracker_LDADD = \
	$(top_builddir)/src/libtracker-control/libtracker-control-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-miner/libtracker-miner-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-data/libtracker-data.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS) \
	$(TRACKER_LIBS)

# Bash completion
if WITH_BASH_COMPLETION
bashcompletiondir = $(BASH_COMPLETION_DIR)
bashcompletion_DATA = bash-completion/tracker
endif

EXTRA_DIST = \
	$(bashcompletion_DATA) \
	meson.build
