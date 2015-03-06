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

bin_PROGRAMS = tracker

tracker_SOURCES = \
	tracker-main.c \
	tracker-compatible.c \
	tracker-compatible.h \
	tracker-config.c \
	tracker-config.h \
	tracker-color.h \
	tracker-daemon.c \
	tracker-daemon.h \
	tracker-dbus.c \
	tracker-dbus.h \
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

install-data-hook: tracker-compat
	$(AM_V_GEN) cp -f ./$^ "$(DESTDIR)${bindir}/tracker-control"
	$(AM_V_GEN) cp -f ./$^ "$(DESTDIR)${bindir}/tracker-import"
	$(AM_V_GEN) cp -f ./$^ "$(DESTDIR)${bindir}/tracker-info"
	$(AM_V_GEN) cp -f ./$^ "$(DESTDIR)${bindir}/tracker-search"
	$(AM_V_GEN) cp -f ./$^ "$(DESTDIR)${bindir}/tracker-sparql"
	$(AM_V_GEN) cp -f ./$^ "$(DESTDIR)${bindir}/tracker-stats"
	$(AM_V_GEN) cp -f ./$^ "$(DESTDIR)${bindir}/tracker-tag"

uninstall-hook:
	$(RM) "$(DESTDIR)$(bindir)/tracker-control" && \
	$(RM) "$(DESTDIR)$(bindir)/tracker-import" && \
	$(RM) "$(DESTDIR)$(bindir)/tracker-info" && \
	$(RM) "$(DESTDIR)$(bindir)/tracker-search" && \
	$(RM) "$(DESTDIR)$(bindir)/tracker-sparql" && \
	$(RM) "$(DESTDIR)$(bindir)/tracker-stats" && \
	$(RM) "$(DESTDIR)$(bindir)/tracker-tag"

# Bash completion
completiondir = $(sysconfdir)/bash_completion.d
completion_DATA = tracker-prompt.sh

tracker-compat: tracker-compat.in
	$(AM_V_at)sed -e "s|@bindir[@]|${bindir}|" $< > $@

EXTRA_DIST = $(completion_DATA) tracker-compat.in
CLEANFILES = tracker-compat
