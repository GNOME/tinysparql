AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-DSHAREDIR=\""$(datadir)"\"                    \
	-DLOCALEDIR=\""$(localedir)"\"                 \
	-DLIBEXEC_PATH=\""$(libexecdir)"\"             \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	$(TRACKER_MINER_RSS_CFLAGS)

libexec_PROGRAMS = tracker-miner-rss

tracker_miner_rss_SOURCES =                            \
	tracker-main.c                                 \
	tracker-miner-rss.h                            \
	tracker-miner-rss.c

tracker_miner_rss_LDADD =                              \
	$(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-miner/libtracker-miner-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS)                                  \
	$(TRACKER_MINER_RSS_LIBS)

# Desktop files: First we fix the VERSION and libexecdir
tracker-miner-rss.desktop.in: tracker-miner-rss.desktop.in.in
	@sed -e "s|@libexecdir[@]|${libexecdir}|" \
	     -e "s|@VERSION[@]|${VERSION}|" $< > $@

desktop_in_in_files = tracker-miner-rss.desktop.in.in
desktop_in_files = tracker-miner-rss.desktop.in

autostartdir = $(sysconfdir)/xdg/autostart
autostart_DATA = tracker-miner-rss.desktop

@INTLTOOL_DESKTOP_RULE@

CLEANFILES = $(autostart_DATA) $(desktop_in_files)
EXTRA_DIST = $(desktop_in_in_files)
