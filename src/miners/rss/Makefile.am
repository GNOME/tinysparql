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

# DBus interfaces (XML files) and services
service_in_files = org.freedesktop.Tracker1.Miner.RSS.service.in

%.service: %.service.in
	@sed -e "s|@libexecdir[@]|${libexecdir}|" $< > $@

servicedir = $(DBUS_SERVICES_DIR)
service_DATA = org.freedesktop.Tracker1.Miner.RSS.service

# Desktop files: First we fix the VERSION and libexecdir
%.desktop.in: %.desktop.in.in
	@sed -e "s|@libexecdir[@]|${libexecdir}|" \
	     -e "s|@VERSION[@]|${VERSION}|" $< > $@

autostartdir = $(sysconfdir)/xdg/autostart
autostart_DATA = tracker-miner-rss.desktop

@INTLTOOL_DESKTOP_RULE@

# Dist / Clean rules for all data
CLEANFILES = \
	$(autostart_DATA) \
	$(autostart_DATA:.desktop=.desktop.in) \
	$(service_DATA)

EXTRA_DIST = \
	$(autostart_DATA:.desktop=.desktop.in.in) \
	$(service_in_files)
